"""
Affected Files Scanner
======================

Purpose
-------
Given a list of changed files, determine which Arduino sketches (.ino) or IDF component examples are affected
and should be rebuilt.

Supports two modes:
- **Default mode**: Analyzes Arduino sketches (.ino files)
- **Component mode** (--component): Analyzes IDF component examples (directories)

High-level workflow
-------------------
1) Discover include roots
   - `include_folders` starts with `cores/esp32` and is extended with every `libraries/*/src` directory.

2) Build forward dependency graph (`dependencies`)
   - **Default mode**: Enumerate source files under `cores/esp32` and `libraries`.
   - **Component mode**: Also includes files under `idf_component_examples`.
   - For each file, parse `#include` lines and resolve headers:
       a) First relative to the including file's directory.
       b) Then under each folder in `include_folders`.
   - Each `.ino` automatically depends on `cores/esp32/Arduino.h`.
   - If a header with the same basename has an adjacent `.c`/`.cpp`, add those as dependencies as well.

3) Optional: augment with Universal Ctags symbol mapping
   - If Universal Ctags is available, the script runs it recursively on `include_folders` with JSON output.
   - It indexes C/C++ function-like symbols (functions and prototypes), capturing scope (namespaces/classes) and signature.
   - Two maps are produced:
       - `ctags_header_to_qnames`: header path -> set of qualified names declared in that header.
       - `ctags_defs_by_qname`: qualified name -> set of implementation source files (.c/.cpp/...).
   - During dependency graph construction, when a header is included, the script finds all qualified names declared
     in that header and adds their implementation files as dependencies of the includer. This connects declarations in
     headers to implementations even when filenames do not match, and correctly handles namespaces/classes/methods via
     qualified names.

4) Build reverse dependency graph (`reverse_dependencies`)
   - For each edge A -> B in `dependencies`, add B -> A in `reverse_dependencies`.

5) Determine affected files
   - **Default mode**: If any file in `build_files` changed, append preset sketches to affected list.
     Start from changed files and walk `reverse_dependencies` breadth-first. Any `.ino` reached is marked as affected.
   - **Component mode**: If any file in `idf_build_files` changed, all IDF component examples are affected.
     If any file in `idf_project_files` changed, only that specific project is affected.
     Walk dependencies to find component example directories containing affected files.

6) Save artifacts
   - `dependencies.json` (forward graph)
   - `dependencies_reverse.json` (reverse graph)
   - `ctags_tags.jsonl` (raw JSONL tag stream)
   - `ctags_header_to_qnames.json` (header path -> set of qualified names declared in that header)
   - `ctags_defs_by_qname.json` (qualified name -> set of implementation source files (.c/.cpp/...))

7) Set CI output
   - If the script is running in a CI environment, set the following environment variables:
     - `recompile_preset`: Whether the preset sketches should be recompiled
     - `should_build`: Whether to build the sketches or component examples
     - `chunks`: A list of chunk indices to build (maximum is defined by `MAX_CHUNKS`)
     - `chunk_count`: The number of chunks to build (maximum is defined by `MAX_CHUNKS`)

Build file patterns
--------------------
- **build_files**: Core Arduino build system files (platform.txt, variants/**, etc.)
- **sketch_build_files**: Sketch-specific files (ci.yml, *.csv in example directories)
- **idf_build_files**: Core IDF build system files (CMakeLists.txt, idf_component.yml, etc.)
- **idf_project_files**: Project-specific IDF files (per-example CMakeLists.txt, sdkconfig, etc.)

Notes on accuracy and limitations
---------------------------------
- Header-only or template-heavy code that defines functions inline in headers will not produce separate implementation
  files. The header remains the dependency.
- If a declaration has no available definition, no implementation edge is added for it.
- Ctags does not require compilation but depends on static parsing. Exotic macro tricks may not be fully resolved.

Usage
-----
python get_affected.py <changed_file1> [changed_file2] ... [--component] [--debug]

Options:
  --component            Analyze IDF component examples instead of Arduino sketches
  --debug                Enable debug messages and save debug artifacts to disk

Output:
- **Default mode**: Prints affected sketches relative file paths to stdout
- **Component mode**: Prints affected IDF component examples relative directory paths to stdout
"""

import sys
import argparse
from pathlib import Path
import os
import re
import queue
import json
import subprocess
import shutil
import fnmatch

script_dir = Path(__file__).resolve().parent
project_root = (script_dir / "../../").resolve()

is_ci = os.environ.get("CI", "false").lower() == "true"
is_pr = os.environ.get("IS_PR", "true").lower() == "true"
max_chunks = int(os.environ.get("MAX_CHUNKS", "15"))
chunk_count = 0

# Whether to analyze component examples instead of sketches.
component_mode = False

# A list of files that are used by the CI and build system.
# If any of these files change, the preset sketches should be recompiled.
build_files = [
    "package.json",
    "tools/get.py",
    "tools/get.exe",
    "platform.txt",
    "programmers.txt",
    ".github/workflows/push.yml",
    ".github/scripts/on-push.sh",
    ".github/scripts/sketch_utils.sh",
    ".github/scripts/get_affected.py",
    ".github/scripts/install-*",
    "variants/esp32*/**",
]

# Files that are used by the sketch build system.
# If any of these files change, the sketch should be recompiled.
sketch_build_files = [
    "libraries/*/examples/**/ci.yml",
    "libraries/*/examples/**/*.csv",
]

# Files that are used by the IDF build system.
# If any of these files change, the IDF examples should be recompiled.
idf_build_files = [
    "CMakeLists.txt",
    "idf_component.yml",
    "Kconfig.projbuild",
    ".github/workflows/build_component.yml",
    ".github/scripts/on-push-idf.sh",
    ".github/scripts/sketch_utils.sh",
    ".github/scripts/get_affected.py",
    ".github/scripts/check-cmakelists.sh",
    "variants/esp32*/**",
]

# Files that are used by the IDF examples.
# If any of these files change, the example that uses them should be recompiled.
idf_project_files = [
    "idf_component_examples/*/CMakeLists.txt",
    "idf_component_examples/*/ci.yml",
    "idf_component_examples/*/*.csv",
    "idf_component_examples/*/sdkconfig*",
    "idf_component_examples/*/main/*",
]

# Preset sketches that are used to test build system changes.
preset_sketch_files = [
    "libraries/NetworkClientSecure/examples/WiFiClientSecure/WiFiClientSecure.ino",
    "libraries/BLE/examples/Server/Server.ino",
    "libraries/ESP32/examples/Camera/CameraWebServer/CameraWebServer.ino",
    "libraries/Insights/examples/MinimalDiagnostics/MinimalDiagnostics.ino",
]

# Whether to recompile the preset sketches for all platforms.
recompile_preset = 0

source_folders = ["cores/esp32", "libraries"]
include_folders = ["cores/esp32"] # This will later be extended to include all library folders.

# A dictionary that maps a file to the files that it depends on.
dependencies = {}

# A dictionary that maps a file to the files that depend on it (reverse dependencies).
reverse_dependencies = {}

# A list of sketches that are affected by the changes.
affected_sketches = []

# A regular expression to match include statements in source files.
include_regex = re.compile(r'#include\s*[<"]([^">]+)[">]')

# Ctags-based symbol index (optional)
ctags_executable = None
ctags_available = False
ctags_header_to_qnames: dict[str, set[str]] = {}
ctags_defs_by_qname: dict[str, set[str]] = {}
header_extensions = {".h", ".hpp", ".hh", ".hxx"}
source_extensions = {".c", ".cc", ".cpp", ".cxx", ".ino"}
skip_extensions = {".md"}

def is_header_file(path: str) -> bool:
    return os.path.splitext(path)[1].lower() in header_extensions

def is_source_file(path: str) -> bool:
    return os.path.splitext(path)[1].lower() in source_extensions

def should_skip_file(path: str) -> bool:
    return os.path.splitext(path)[1].lower() in skip_extensions or path.startswith("tests/") or path.startswith("variants/")

def detect_universal_ctags() -> bool:
    """
    Return True if Universal Ctags is available on PATH.
    """
    exec_names = ["ctags-universal", "ctags"]

    global ctags_executable
    for exec_name in exec_names:
        ctags_path = shutil.which(exec_name)
        if ctags_path:
            break
    if not ctags_path:
        return False

    ctags_executable = ctags_path

    try:
        out = subprocess.run([ctags_executable, "--version"], capture_output=True, text=True, check=False)
        return "Universal Ctags" in (out.stdout + out.stderr)
    except Exception:
        return False

def normalize_function_signature(signature: str) -> str:
    """
    Normalize a function signature by removing parameter names, keeping only types.

    This handles cases where header declarations and implementations have different parameter names.
    Uses a simple heuristic: the last word in each parameter is typically the parameter name.

    For example:
    - "ltoa(long val, char *s, int radix)" -> "ltoa(long,char *,int)"
    - "ltoa(long value, char *result, int base)" -> "ltoa(long,char *,int)"

    Args:
        signature: The function signature string, e.g., "(long val, char *s, int radix)"

    Returns:
        Normalized signature with parameter names removed, e.g., "(long,char *,int)"
    """
    if not signature:
        return signature

    # Normalize signatures: treat (void) and () as equivalent (both mean no parameters)
    if signature == "(void)":
        return "()"

    if not (signature.startswith("(") and signature.endswith(")")):
        return signature

    # Handle const qualifier at the end (e.g., "(int i) const")
    const_qualifier = ""
    if signature.endswith(" const"):
        signature = signature[:-6]  # Remove " const"
        const_qualifier = " const"

    # Extract parameter list without parentheses
    param_list = signature[1:-1]
    if not param_list.strip():
        return "()" + const_qualifier

    # Split by comma and process each parameter
    params = []
    for param in param_list.split(","):
        param = param.strip()
        if not param:
            continue

        # Handle default parameters (e.g., "int x = 5")
        if "=" in param:
            param = param.split("=")[0].strip()

        # Try simple approach first: remove the last word
        # This works for most cases: "int x" -> "int", "MyStruct s" -> "MyStruct"
        import re

        # Handle arrays first: "int arr[10]" -> "int [10]", "char *argv[]" -> "char *[]"
        array_match = re.match(r'^(.+?)\s+(\w+)((?:\[\d*\])+)$', param)
        if array_match:
            type_part = array_match.group(1).strip()
            array_brackets = array_match.group(3)
            params.append(type_part + array_brackets)
        else:
            # Handle function pointers: "int (*func)(int, char)" -> "int (*)(int, char)"
            func_ptr_match = re.match(r'^(.+?)\s*\(\s*\*\s*(\w+)\s*\)\s*\((.+?)\)\s*$', param)
            if func_ptr_match:
                return_type = func_ptr_match.group(1).strip()
                inner_params = func_ptr_match.group(3).strip()
                # Recursively normalize the inner parameters
                if inner_params:
                    inner_normalized = normalize_function_signature(f"({inner_params})")
                    inner_normalized = inner_normalized[1:-1]  # Remove outer parentheses
                else:
                    inner_normalized = ""
                params.append(f"{return_type} (*)({inner_normalized})")
            else:
                # Try simple approach: remove the last word
                simple_match = re.match(r'^(.+)\s+(\w+)$', param)
                if simple_match:
                    # Simple case worked - just remove the last word
                    type_part = simple_match.group(1).strip()
                    params.append(type_part)
                else:
                    # Fallback to complex regex for edge cases with pointers
                    # First, try to match cases with pointers/references (including multiple *)
                    # Pattern: (everything before) (one or more * or &) (space) (parameter name)
                    m = re.match(r'^(.+?)(\s*[*&]+\s+)(\w+)$', param)
                    if m:
                        # Keep everything before the pointers, plus the pointers (without the space before param name)
                        type_part = m.group(1) + m.group(2).rstrip()
                        params.append(type_part.strip())
                    else:
                        # Try to match cases without space between type and pointer: "char*ptr", "char**ptr"
                        m = re.match(r'^(.+?)([*&]+)(\w+)$', param)
                        if m:
                            # Keep everything before the pointers, plus the pointers
                            type_part = m.group(1) + m.group(2)
                            params.append(type_part.strip())
                        else:
                            # Single word - assume it's a type
                            params.append(param.strip())

        # Normalize spacing around pointers to ensure consistent output
        # This ensures "char *" and "char*" both become "char *"
        if params:
            last_param = params[-1]
            # Normalize spacing around * and & symbols
            # Replace multiple spaces with single space, ensure space before * and &
            normalized = re.sub(r'\s+', ' ', last_param)  # Collapse multiple spaces
            normalized = re.sub(r'\s*([*&]+)', r' \1', normalized)  # Ensure space before * and &
            normalized = re.sub(r'([*&]+)\s*', r'\1 ', normalized)  # Ensure space after * and &
            normalized = re.sub(r'\s+', ' ', normalized).strip()  # Clean up extra spaces
            params[-1] = normalized

    result = "(" + ",".join(params) + ")"
    if const_qualifier:
        result += const_qualifier

    return result

def build_qname_from_tag(tag: dict) -> str:
    """
    Compose a qualified name for a function/method using scope + name + signature.
    Falls back gracefully if some fields are missing.
    """
    scope = tag.get("scope") or ""
    name = tag.get("name") or ""
    signature = tag.get("signature") or ""

    # Normalize the signature to remove parameter names
    signature = normalize_function_signature(signature)

    qparts = []
    if scope:
        qparts.append(scope)
    qparts.append(name)
    qname = "::".join([p for p in qparts if p])
    return f"{qname}{signature}"

def find_impl_files_for_qname(qname: str, defs_by_qname: dict[str, set[str]], header_path: str = None) -> set[str]:
    """
    Find implementation files for a qualified name, handling namespace mismatches.

    Ctags may capture different namespace scopes in headers vs implementations.
    For example:
    - Header: fs::SDFS::begin(...)
    - Implementation: SDFS::begin(...)

    This happens when implementations use "using namespace" directives.

    Strategy:
    1. Try exact match first
    2. If no match and qname has namespaces, try stripping ONLY outer namespace prefixes
       (keep at least Class::method structure intact)
    3. If header_path provided, prefer implementations from same directory
    """
    # Try exact match first
    impl_files = defs_by_qname.get(qname, set())
    if impl_files:
        return impl_files

    # If no exact match and the qname contains namespaces (::), try stripping them
    if "::" in qname:
        parts = qname.split("::")
        # Only strip outer namespaces, not the class/method structure
        # For "ns1::ns2::Class::method(...)", we want to try:
        # - "ns2::Class::method(...)"  (strip 1 level)
        # - "Class::method(...)"       (strip 2 levels)
        # But NOT "method(...)" alone (too ambiguous)

        # Only allow stripping if we have more than 2 parts (namespace::Class::method)
        # If we only have 2 parts (Class::method), don't strip as it would leave just "method"
        if len(parts) > 2:
            # Keep at least 2 parts (Class::method) to avoid false positives
            max_strip = len(parts) - 2

            for i in range(1, max_strip + 1):
                shorter_qname = "::".join(parts[i:])
                impl_files = defs_by_qname.get(shorter_qname, set())

                if impl_files:
                    # If we have the header path, prefer implementations from same directory
                    if header_path:
                        header_dir = os.path.dirname(header_path)
                        same_dir_files = {f for f in impl_files if os.path.dirname(f) == header_dir}
                        if same_dir_files:
                            return same_dir_files

                    return impl_files

    return set()

def run_ctags_and_index(paths: list[str]) -> tuple[dict[str, set[str]], dict[str, set[str]], str]:
    """
    Run Universal Ctags over given paths (relative to project_root) and build:
      - header_to_qnames: header path -> set of qualified names declared in that header
      - defs_by_qname: qualified name -> set of source files where it is defined
    Paths in results are relative to project_root to match this script's expectations.
    """
    cmd = [
        ctags_executable, "-R", "-f", "-",
        "--map-C++=+.ino",
        "--languages=C,C++",
        "--kinds-C=+p",
        "--kinds-C++=+pf",
        "--fields=+nKSt+s",
        "--output-format=json",
    ] + paths

    try:
        proc = subprocess.run(
            cmd,
            cwd=project_root,
            capture_output=True,
            text=True,
            check=True,
        )
        print(f"Ctags completed successfully. Output length: {len(proc.stdout)}", file=sys.stderr)
    except subprocess.CalledProcessError as e:
        print(f"Error: Ctags failed with return code {e.returncode}", file=sys.stderr)
        print(f"Ctags stdout: {e.stdout}", file=sys.stderr)
        print(f"Ctags stderr: {e.stderr}", file=sys.stderr)
        return {}, {}, ""
    except Exception as e:
        print(f"Warning: Failed to run ctags: {e}", file=sys.stderr)
        return {}, {}, ""

    header_to_qnames: dict[str, set[str]] = {}
    defs_by_qname: dict[str, set[str]] = {}

    for line in proc.stdout.splitlines():
        line = line.strip()
        if not line or not line.startswith("{"):
            continue
        try:
            tag = json.loads(line)
        except Exception:
            continue

        path = tag.get("path")
        if not path:
            continue

        # Only consider C/C++ function-like symbols
        kind = tag.get("kind")
        # Accept both long names and single-letter kinds from ctags
        is_function_like = kind in ("function", "prototype") or kind in ("f", "p")
        if not is_function_like:
            continue

        qname = build_qname_from_tag(tag)
        if not qname:
            continue

        if is_header_file(path):
            header_to_qnames.setdefault(path, set()).add(qname)
        elif is_source_file(path):
            defs_by_qname.setdefault(qname, set()).add(path)

    print(f"Parsed {len(header_to_qnames)} headers and {len(defs_by_qname)} symbol definitions", file=sys.stderr)
    return header_to_qnames, defs_by_qname, proc.stdout

def save_ctags_artifacts(raw_jsonl: str, header_to_qnames: dict[str, set[str]], defs_by_qname: dict[str, set[str]]) -> None:
    """
    Save ctags artifacts to disk:
      - Raw JSONL tags stream (ctags_tags.jsonl)
      - Header to qualified names map (ctags_header_to_qnames.json)
      - Qualified name to implementation files map (ctags_defs_by_qname.json)
    """
    try:
        with open("ctags_tags.jsonl", "w", encoding="utf-8") as f:
            f.write(raw_jsonl)
        # Convert sets to sorted lists for JSON serialization
        hdr_json = {k: sorted(list(v)) for k, v in header_to_qnames.items()}
        defs_json = {k: sorted(list(v)) for k, v in defs_by_qname.items()}
        with open("ctags_header_to_qnames.json", "w", encoding="utf-8") as f:
            json.dump(hdr_json, f, indent=2, sort_keys=True)
        with open("ctags_defs_by_qname.json", "w", encoding="utf-8") as f:
            json.dump(defs_json, f, indent=2, sort_keys=True)
        print("Ctags artifacts saved: ctags_tags.jsonl, ctags_header_to_qnames.json, ctags_defs_by_qname.json", file=sys.stderr)
    except Exception as e:
        print(f"Warning: Failed to save ctags artifacts: {e}", file=sys.stderr)

def find_library_folders() -> list[str]:
    """
    Find all library src folders and append them to include_folders.
    """
    libraries_path = project_root / "libraries"
    if libraries_path.exists():
        for lib_dir in libraries_path.iterdir():
            if lib_dir.is_dir():
                src_path = lib_dir / "src"
                if src_path.is_dir():
                    include_folders.append(str(src_path.relative_to(project_root)))

def list_files() -> list[str]:
    """
    List all source files in the project as relative paths to project_root.
    Only search in specific folders to avoid processing the entire project.
    """
    files = []
    for folder in source_folders:
        folder_path = os.path.join(project_root, folder)
        if os.path.exists(folder_path):
            for root, dirs, file_list in os.walk(folder_path):
                for file in file_list:
                    if is_source_file(file) or is_header_file(file):
                        abs_path = os.path.join(root, file)
                        rel_path = os.path.relpath(abs_path, project_root)
                        files.append(str(rel_path))
    return files

def list_ino_files() -> list[str]:
    """
    List all .ino files in the project.
    """
    files = []
    for file in list_files():
        if file.endswith('.ino'):
            files.append(file)
    return files

def list_idf_component_examples() -> list[str]:
    """
    List all IDF component example directories in the project.
    """
    examples = []
    examples_path = project_root / "idf_component_examples"
    if examples_path.exists():
        for example_dir in examples_path.iterdir():
            if example_dir.is_dir():
                examples.append(str(example_dir.relative_to(project_root)))
    return examples

def search_file(filepath: str, folders: list[str]) -> str:
    """
    Search the filepath using the folders as root folders.
    If the file is found, return the full path, otherwise return None.
    """
    for folder in folders:
        full_path = os.path.join(folder, filepath)
        if os.path.exists(full_path):
            return full_path
    return None

def resolve_include_path(include_file: str, current_file: str) -> str:
    """
    Resolve an include statement to a full file path.
    """

    # First try relative to the current file's directory
    current_dir = os.path.dirname(current_file)
    if current_dir:
        relative_path = os.path.join(current_dir, include_file)
        if os.path.exists(os.path.join(project_root, relative_path)):
            return relative_path

    # Then try to find the file in the include folders
    resolved_path = search_file(include_file, include_folders)
    if resolved_path:
        return os.path.relpath(resolved_path, project_root)

    return None

def build_dependencies_graph() -> None:
    """
    Build a dependency graph for the project.
    """
    q = queue.Queue()
    for file in list_ino_files():
        q.put(file)
    for file in list_files():
        dependencies[file] = []

    processed_files = set()

    while not q.empty():
        file = q.get()
        if file in processed_files:
            continue

        processed_files.add(file)
        file_path = os.path.join(project_root, file)

        # Automatically add Arduino.h dependency for .ino files
        if file.endswith('.ino'):
            dependencies[file].append('cores/esp32/Arduino.h')

        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
        except Exception as e:
            print(f"Warning: Could not read file {file}: {e}", file=sys.stderr)
            continue

        for match in include_regex.finditer(content):
            include_file = match.group(1)
            if include_file.endswith('.h'):
                header_path = resolve_include_path(include_file, file)

                # If the header file is found
                if header_path:
                    # Add header file to dependencies
                    if header_path not in dependencies[file]:
                        dependencies[file].append(header_path)
                        # Only add to queue if it's a source file we haven't processed yet
                        if header_path in dependencies and header_path not in processed_files:
                            q.put(header_path)

                    # Try to use ctags to match header declarations to implementation files
                    if ctags_available:
                        # Get qualified names of functions and methods declared in the header
                        qnames = ctags_header_to_qnames.get(header_path)
                        if qnames:
                            impl_files = set()
                            for qn in qnames:
                                # For each qualified name, find implementation files
                                # This handles namespace mismatches (e.g., fs::SDFS vs SDFS)
                                # Pass header_path to prefer implementations from same directory
                                impl_files |= find_impl_files_for_qname(qn, ctags_defs_by_qname, header_path)
                            for impl in impl_files:
                                # Skip .ino files - they should never be dependencies of other files
                                if impl.endswith('.ino'):
                                    continue
                                # Add implementation file to dependencies
                                if impl not in dependencies[file]:
                                    dependencies[file].append(impl)
                                    if impl in dependencies and impl not in processed_files:
                                        q.put(impl)
                    else:
                        # Fall back to resolving .cpp and .c files
                        cpp_path = resolve_include_path(include_file.replace('.h', '.cpp'), file)
                        c_path = resolve_include_path(include_file.replace('.h', '.c'), file)

                        # Add C++ file to dependencies if it exists
                        if cpp_path and cpp_path not in dependencies[file]:
                            dependencies[file].append(cpp_path)
                            if cpp_path in dependencies and cpp_path not in processed_files:
                                q.put(cpp_path)

                        # Add C file to dependencies if it exists
                        if c_path and c_path not in dependencies[file]:
                            dependencies[file].append(c_path)
                            if c_path in dependencies and c_path not in processed_files:
                                q.put(c_path)


def build_reverse_dependencies() -> None:
    """
    Build a reverse dependency graph showing which files depend on each file.
    """
    for file, deps in dependencies.items():
        for dependency in deps:
            if dependency not in reverse_dependencies:
                reverse_dependencies[dependency] = []
            reverse_dependencies[dependency].append(file)

def has_build_files_changed(changed_files: list[str]) -> bool:
    """
    Check if any of the build files have changed.
    Supports glob patterns in build_files.
    """
    for file in changed_files:
        if should_skip_file(file):
            continue

        if not component_mode:
            for pattern in build_files:
                if fnmatch.fnmatch(file, pattern):
                    return True
        else:
            for pattern in idf_build_files:
                if fnmatch.fnmatch(file, pattern):
                    return True

    return False

def has_sketch_build_files_changed(path: str) -> bool:
    """
    Check if any of the sketch build files have changed.
    """
    if should_skip_file(path):
        return False

    if not component_mode:
        for pattern in sketch_build_files:
            if fnmatch.fnmatch(path, pattern):
                return True
    else:
        for pattern in idf_project_files:
            if fnmatch.fnmatch(path, pattern):
                return True
    return False

def preprocess_changed_files(changed_files: list[str]) -> None:
    """
    Preprocess the changed files to detect build system changes.
    """
    # Special case: if the Arduino libs or tools changes, rebuild all sketches on PRs
    if is_pr and not component_mode:
        for changed in changed_files:
            if changed == "package/package_esp32_index.template.json":
                print("Package index changed - all sketches affected", file=sys.stderr)
                all_sketches = list_ino_files()
                for sketch in all_sketches:
                    if sketch not in affected_sketches:
                        affected_sketches.append(sketch)
                # No need to continue preprocessing; dependency walk will still run afterwards
                break
    # If not a PR, skip preprocessing as we'll recompile everything anyway
    if not is_pr:
        return

    # Check if any build files have changed
    if has_build_files_changed(changed_files):
        if component_mode:
            # If IDF build files changed, build all IDF component examples
            print("IDF build files changed - all component examples affected", file=sys.stderr)
            for example in list_idf_component_examples():
                if example not in affected_sketches:
                    affected_sketches.append(example)
        else:
            # If build files changed, build the preset sketches
            print("Build files changed - preset sketches affected", file=sys.stderr)
            global recompile_preset
            recompile_preset = 1
            for file in preset_sketch_files:
                if file not in affected_sketches:
                    affected_sketches.append(file)

    for file in changed_files:
        if has_sketch_build_files_changed(file):
            if component_mode:
                # Extract the project name from idf_project_files pattern
                # e.g., "idf_component_examples/hello_world/CMakeLists.txt" -> "idf_component_examples/hello_world"
                if file.startswith("idf_component_examples/"):
                    parts = file.split("/")
                    if len(parts) >= 2:
                        project_path = "/".join(parts[:2])  # idf_component_examples/project_name
                        if project_path not in affected_sketches:
                            print(f"IDF project file changed - {project_path} affected", file=sys.stderr)
                            affected_sketches.append(project_path)
            else:
                dir_name = os.path.dirname(file)
                sketch_name = dir_name + "/" + os.path.basename(dir_name) + ".ino"
                if sketch_name not in affected_sketches:
                    affected_sketches.append(sketch_name)

def find_affected_sketches(changed_files: list[str]) -> None:
    """
    Find the sketches that are affected by the changes.
    """

    # If not a PR, recompile everything
    if not is_pr:
        if component_mode:
            print("Not a PR - recompiling all IDF component examples", file=sys.stderr)
            all_examples = list_idf_component_examples()
            for example in all_examples:
                if example not in affected_sketches:
                    affected_sketches.append(example)
            print(f"Total affected IDF component examples: {len(affected_sketches)}", file=sys.stderr)
        else:
            print("Not a PR - recompiling all sketches", file=sys.stderr)
            all_sketches = list_ino_files()
            for sketch in all_sketches:
                if sketch not in affected_sketches:
                    affected_sketches.append(sketch)
            print(f"Total affected sketches: {len(affected_sketches)}", file=sys.stderr)
        return

    # For component mode: if any *source code* file (not example or documentation) changed, recompile all examples
    if component_mode:
        for file in changed_files:
            if (is_source_file(file) or is_header_file(file)) and not file.endswith(".ino"):
                if file.startswith("cores/") or file.startswith("libraries/"):
                    print("Component mode: file changed in cores/ or libraries/ - recompiling all IDF component examples", file=sys.stderr)
                    all_examples = list_idf_component_examples()
                    for example in all_examples:
                        if example not in affected_sketches:
                            affected_sketches.append(example)
                    print(f"Total affected IDF component examples: {len(affected_sketches)}", file=sys.stderr)
                    return

    preprocess_changed_files(changed_files)

    # Normal dependency-based analysis for non-critical changes
    processed_files = set()
    q = queue.Queue()

    if component_mode:
        # Get all available component examples once for efficiency
        all_examples = list_idf_component_examples()
    else:
        all_examples = []

    for file in changed_files:
        if not should_skip_file(file):
            q.put(file)

    while not q.empty():
        file = q.get()
        if file in processed_files:
            continue

        processed_files.add(file)

        if component_mode:
            # Check if this file belongs to an IDF component example
            for example in all_examples:
                if file.startswith(example + "/") and example not in affected_sketches:
                    affected_sketches.append(example)
        else:
            if file.endswith('.ino') and file not in affected_sketches:
                affected_sketches.append(file)

        # Continue with reverse dependency traversal
        if file in reverse_dependencies:
            for dependency in reverse_dependencies[file]:
                if dependency not in processed_files:
                    if component_mode:
                        # In component mode, only follow dependencies within the same example
                        # IDF component examples are isolated - no cross-example dependencies
                        current_example = None
                        dependency_example = None

                        # Find which examples the current file and dependency belong to
                        for example in all_examples:
                            if file.startswith(example + "/"):
                                current_example = example
                            if dependency.startswith(example + "/"):
                                dependency_example = example

                        # Traverse dependencies based on context:
                        # 1. If current file is NOT in any example (shared/core file), allow traversal to any example
                        # 2. If current file IS in an example, only traverse within the same example
                        should_traverse = False

                        if not current_example:
                            # Current file is a shared/core file - can affect any example
                            should_traverse = True
                        elif current_example == dependency_example:
                            # Both files are in the same component example
                            should_traverse = True

                        if should_traverse:
                            q.put(dependency)
                            if dependency_example and dependency_example not in affected_sketches:
                                affected_sketches.append(dependency_example)
                    else:
                        q.put(dependency)
                        if dependency.endswith('.ino') and dependency not in affected_sketches:
                            affected_sketches.append(dependency)

    if component_mode:
        print(f"Total affected IDF component examples: {len(affected_sketches)}", file=sys.stderr)
        if affected_sketches:
            print("Affected IDF component examples:", file=sys.stderr)
            for example in affected_sketches:
                print(f"  {example}", file=sys.stderr)
    else:
        print(f"Total affected sketches: {len(affected_sketches)}", file=sys.stderr)
        if affected_sketches:
            print("Affected sketches:", file=sys.stderr)
            for sketch in affected_sketches:
                print(f"  {sketch}", file=sys.stderr)

def save_dependencies_as_json(output_file: str = "dependencies.json") -> None:
    """
    Save both the forward and reverse dependency graphs as JSON files.
    """
    # Save forward dependencies
    forward_deps_file = output_file
    with open(forward_deps_file, 'w') as f:
        json.dump(dependencies, f, indent=2, sort_keys=True)
    print(f"Forward dependencies saved to: {forward_deps_file}", file=sys.stderr)

    # Save reverse dependencies
    reverse_deps_file = output_file.replace('.json', '_reverse.json')
    with open(reverse_deps_file, 'w') as f:
        json.dump(reverse_dependencies, f, indent=2, sort_keys=True)
    print(f"Reverse dependencies saved to: {reverse_deps_file}", file=sys.stderr)

def check_preset_files_affected():
    """
    Check if any of the preset sketch files are in the affected sketches list.
    If so, set recompile_preset to True.
    """
    global recompile_preset

    # If not a PR, always recompile preset sketches
    if not is_pr:
        if not component_mode:  # Only check preset files in sketch mode
            print("Not a PR - setting recompile_preset=1 for all preset sketches", file=sys.stderr)
            recompile_preset = 1
        return

    if not component_mode:  # Only check preset files in sketch mode
        for preset_file in preset_sketch_files:
            if preset_file in affected_sketches:
                print(f"Preset sketches affected - setting recompile_preset=1", file=sys.stderr)
                recompile_preset = 1
                break

def set_ci_output(print_vars: bool=True):
    """
    Set GITHUB_OUTPUT environment variable
    """
    # Determine output target
    if not is_ci and print_vars:
        print("Not a CI environment - printing variables to stdout:", file=sys.stderr)
        f = sys.stderr
    elif is_ci:
        out = os.environ["GITHUB_OUTPUT"]
        f = open(out, "a")
    else:
        return

    try:
        # Write all output values
        f.write(f"recompile_preset={recompile_preset}\n")
        sketch_count = len(affected_sketches)
        if sketch_count > 0:
            f.write(f"should_build=1\n")
            # Calculate optimal chunk count to avoid creating empty chunks
            # Use the same algorithm as sketch_utils.sh to determine how many chunks are actually needed
            if sketch_count <= max_chunks:
                # If we have fewer sketches than max chunks, use one chunk per sketch
                chunk_count = sketch_count
                print(f"Creating {chunk_count} chunks for {sketch_count} sketches (one per sketch).", file=sys.stderr)
            else:
                # If we have more sketches than max chunks, distribute across max_chunks
                # Calculate chunk size: ceil(sketch_count / max_chunks)
                chunk_size = (sketch_count + max_chunks - 1) // max_chunks  # ceiling division
                # Calculate actual chunks needed
                chunks_needed = (sketch_count + chunk_size - 1) // chunk_size  # ceiling division
                chunk_count = min(chunks_needed, max_chunks)
                print(f"More sketches ({sketch_count}) than max chunks ({max_chunks}). Using {chunk_count} chunks with ~{chunk_size} sketches each.", file=sys.stderr)
            
            chunks='["0"'
            for i in range(1, chunk_count):
                chunks+=f",\"{i}\""
            chunks+="]"
            f.write(f"chunks={chunks}\n")
        else:
            f.write(f"should_build=0\n")
            chunk_count = 0
        f.write(f"chunk_count={chunk_count}\n")
    finally:
        # Close file if we opened it (CI mode)
        if is_ci:
            f.close()

def main():
    parser = argparse.ArgumentParser(description="Affected Sketches Scanner")
    parser.add_argument("changed_files", nargs="+", help="List of changed files (e.g., file1.cpp file2.h file3.cpp)")
    parser.add_argument("--component", action="store_true", help="Get affected component examples instead of sketches")
    parser.add_argument("--debug", action="store_true", help="Enable debug messages and save debug artifacts to disk")
    args = parser.parse_args()

    changed_files = args.changed_files
    global component_mode
    component_mode = args.component
    if component_mode:
        print(f"Analyzing IDF component examples...", file=sys.stderr)
        source_folders.append("idf_component_examples")
    else:
        print(f"Analyzing sketches...", file=sys.stderr)

    print(f"Finding include folders...", file=sys.stderr)
    find_library_folders()
    print(f"Found {len(include_folders)} include folders", file=sys.stderr)

    # Initialize ctags-based symbol index if available
    global ctags_available, ctags_header_to_qnames, ctags_defs_by_qname
    ctags_available = detect_universal_ctags()
    if ctags_available:
        print("Universal Ctags detected - indexing symbols for header-to-implementation mapping...", file=sys.stderr)
        try:
            hdr_to_qn, defs_by_qn, raw_jsonl = run_ctags_and_index(source_folders)
            ctags_header_to_qnames, ctags_defs_by_qname = hdr_to_qn, defs_by_qn
            print(f"Ctags index built: {len(ctags_header_to_qnames)} headers, {len(ctags_defs_by_qname)} symbols", file=sys.stderr)
            if args.debug:
                print("Saving ctags artifacts...", file=sys.stderr)
                save_ctags_artifacts(raw_jsonl, ctags_header_to_qnames, ctags_defs_by_qname)
        except Exception as e:
            print(f"Warning: Ctags indexing failed: {e}", file=sys.stderr)
            ctags_available = False
    else:
        print("Universal Ctags not found - proceeding without symbol-based mapping", file=sys.stderr)

    print(f"Building dependency graph...", file=sys.stderr)
    build_dependencies_graph()
    print(f"Processed {len(dependencies)} files", file=sys.stderr)

    print(f"Building reverse dependency graph...", file=sys.stderr)
    build_reverse_dependencies()

    # Save dependencies as JSON artifacts
    if args.debug:
        print(f"Saving dependencies as JSON artifacts...", file=sys.stderr)
        save_dependencies_as_json()

    print(f"Finding affected sketches...", file=sys.stderr)
    if not is_pr:
        print("Not a PR - will recompile everything", file=sys.stderr)
    find_affected_sketches(changed_files)

    print(f"Checking if preset sketch files were affected...", file=sys.stderr)
    check_preset_files_affected()

    print(f"Printing affected sketches...", file=sys.stderr)
    for sketch in affected_sketches:
        print(sketch)

    set_ci_output(args.debug)

if __name__ == "__main__":
    main()
