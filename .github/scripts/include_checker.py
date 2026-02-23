#!/usr/bin/env python3
"""
Arduino Include Checker
Scans .ino files and ensures they have #include <Arduino.h> before preprocessor directives
"""

import sys
import re
import argparse
from pathlib import Path
from typing import List, Tuple


def has_arduino_include(content: str) -> bool:
    """Check if content already has Arduino.h include"""
    patterns = [
        r'#include\s*<Arduino\.h>',
        r'#include\s*"Arduino\.h"'
    ]
    for pattern in patterns:
        if re.search(pattern, content):
            return True
    return False


def find_first_preprocessor_line(lines: List[str]) -> int:
    """
    Find the index of the first preprocessor directive (line starting with #)
    Returns -1 if no preprocessor directive found
    """
    for i, line in enumerate(lines):
        stripped = line.lstrip()
        if stripped.startswith('#'):
            return i
    return -1


def add_arduino_include(filepath: Path, verbose: bool = False) -> Tuple[bool, str]:
    """
    Add #include <Arduino.h> to the file before the first preprocessor directive
    Returns (success, message)
    """
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()

        # Check if already has Arduino.h include
        if has_arduino_include(content):
            return True, "Already has Arduino.h include"

        lines = content.split('\n')
        first_directive_idx = find_first_preprocessor_line(lines)

        if first_directive_idx == -1:
            return False, "No preprocessor directive found"

        # Insert the include before the first directive
        lines.insert(first_directive_idx, '#include <Arduino.h>')

        # Write back to file
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write('\n'.join(lines))

        return True, "Added #include <Arduino.h>"

    except Exception as e:
        return False, f"Error processing file: {str(e)}"


def scan_ino_files(root_dir: str, verbose: bool = False) -> Tuple[List[Path], List[Path], List[Tuple[Path, str]]]:
    """
    Scan directory for .ino files and process them
    Returns (already_had_include, files_added, failed_files_with_reasons)
    """
    already_had = []
    added = []
    failed = []

    root_path = Path(root_dir)
    if not root_path.exists():
        print(f"Error: Directory '{root_dir}' does not exist")
        sys.exit(1)

    # Find all .ino files
    ino_files = list(root_path.rglob('*.ino'))

    if not ino_files:
        if verbose:
            print(f"No .ino files found in '{root_dir}'")
        return already_had, added, failed

    if verbose:
        print(f"Found {len(ino_files)} .ino file(s)")
        print("-" * 60)

    for filepath in ino_files:
        if verbose:
            print(f"\nProcessing: {filepath.relative_to(root_path)}")

        success, message = add_arduino_include(filepath, verbose)

        if verbose:
            print(f"  → {message}")

        if success:
            if "Already has" in message:
                already_had.append(filepath)
            else:
                added.append(filepath)
        else:
            failed.append((filepath, message))

    return already_had, added, failed


def process_individual_files(filepaths: List[str], verbose: bool = False) -> Tuple[List[Path], List[Path], List[Tuple[Path, str]]]:
    """
    Process individual .ino files (used by pre-commit hooks)
    Returns (already_had_include, files_added, failed_files_with_reasons)
    """
    already_had = []
    added = []
    failed = []

    if verbose:
        print(f"Processing {len(filepaths)} file(s)")
        print("-" * 60)

    for filepath_str in filepaths:
        filepath = Path(filepath_str).resolve()

        if not filepath.exists():
            if verbose:
                print(f"\n⚠️  File not found: {filepath}")
            failed.append((filepath, "File not found"))
            continue

        if not filepath.suffix == '.ino':
            if verbose:
                print(f"\n⚠️  Skipping non-.ino file: {filepath}")
            continue

        if verbose:
            print(f"\nProcessing: {filepath}")

        success, message = add_arduino_include(filepath, verbose)

        if verbose:
            print(f"  → {message}")

        if success:
            if "Already has" in message:
                already_had.append(filepath)
            else:
                added.append(filepath)
        else:
            failed.append((filepath, message))

    return already_had, added, failed


def main():
    parser = argparse.ArgumentParser(
        description='Check and fix Arduino.h includes in .ino files',
        epilog='Exit code 0: All files OK. Exit code 1: Files modified or failed.'
    )
    parser.add_argument(
        'paths',
        nargs='*',
        help='Directory to scan or specific .ino files to check (default: two levels up from script)'
    )
    parser.add_argument(
        '-v', '--verbose',
        action='store_true',
        help='Show detailed processing information'
    )

    args = parser.parse_args()

    # Determine what to process
    if not args.paths:
        # Default to project root (two levels up from script location)
        script_dir = Path(__file__).resolve().parent
        project_root = (script_dir / "../../").resolve()
        root_dir = str(project_root)
        if args.verbose:
            print(f"No directory specified, using default: {root_dir}")
            print(f"Scanning directory: {root_dir}")
            print("=" * 60)
        already_had, added, failed = scan_ino_files(root_dir, args.verbose)
    else:
        # Check if first argument is a directory or a file
        first_path = Path(args.paths[0])

        if first_path.is_dir():
            # Directory mode: scan directory
            root_dir = args.paths[0]
            if args.verbose:
                print(f"Scanning directory: {root_dir}")
                print("=" * 60)
            already_had, added, failed = scan_ino_files(root_dir, args.verbose)
        else:
            # File mode: process individual files (pre-commit hook mode)
            if args.verbose:
                print("Running in pre-commit hook mode")
                print("=" * 60)
            already_had, added, failed = process_individual_files(args.paths, args.verbose)

    # Only show summary in verbose mode or if there are issues
    if args.verbose:
        print("\n" + "=" * 60)
        print("SUMMARY")
        print("=" * 60)
        print(f"Total files found: {len(already_had) + len(added) + len(failed)}")
        print(f"Already had Arduino.h: {len(already_had)}")
        print(f"Arduino.h added: {len(added)}")
        print(f"Failed: {len(failed)}")

    # Always show files that were modified
    if added:
        if not args.verbose:
            print("Arduino.h was added to the following file(s):")
        else:
            print(f"\n✓ Arduino.h was added to {len(added)} file(s):")
        for filepath in added:
            print(f"  • {filepath}")

    # Always show files that failed
    if failed:
        if not args.verbose:
            print("\nFailed to add include to the following file(s):")
        else:
            print(f"\n⚠️  Failed to add include to {len(failed)} file(s):")
        for filepath, reason in failed:
            print(f"  • {filepath}")
            print(f"    Reason: {reason}")

    # Exit with code 1 if any files were modified or failed
    if added or failed:
        sys.exit(1)
    else:
        if args.verbose:
            print("\n✓ All .ino files already have Arduino.h include!")
        sys.exit(0)


if __name__ == "__main__":
    main()
