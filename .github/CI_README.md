# ESP32 Arduino Core CI/CD System

This document explains how the Continuous Integration and Continuous Deployment (CI/CD) system works for the ESP32 Arduino Core project.

## Table of Contents

- [Overview](#overview)
- [Centralized SoC Configuration](#centralized-soc-configuration)
- [Workflows](#workflows)
  - [Compilation Tests](#compilation-tests-pushyml)
  - [Runtime Tests](#runtime-tests)
  - [Multi-Device (Multi-DUT) Tests](#multi-device-multi-dut-tests)
  - [IDF Component Build](#idf-component-build-build_componentyml)
  - [Board Validation](#board-validation-boardsyml)
  - [Library Tests](#library-tests-libyml)
  - [All Boards Test](#all-boards-test-allboardsyml)
  - [Documentation](#documentation-docs_buildyml-docs_deployyml)
  - [Release](#release-releaseyml)
  - [Size Reporting](#size-reporting-publishsizesyml-publishsizes-2xyml)
  - [Pre-commit](#pre-commit-pre-commityml-pre-commit-statusyml)
  - [CodeQL Security](#codeql-security-codeqlyml)
  - [Build Python Tools](#build-python-tools-build_py_toolsyml)
  - [Backlog Bot](#backlog-bot-backlog-botyml)
- [Scripts](#scripts)
- [Adding a New SoC](#adding-a-new-soc)
- [Official Variants Filter](#official-variants-filter)
- [Testing](#testing)

---

## Overview

The CI/CD system is designed to:
- Build and test Arduino sketches across multiple ESP32 SoC variants
- Test on physical hardware, Wokwi simulator, and QEMU emulator
- Build ESP-IDF component examples
- Validate board definitions
- Test external libraries
- Generate documentation
- Publish releases

All SoC definitions are centralized in `.github/scripts/socs_config.sh` to ensure consistency and ease of maintenance.

---

## Centralized SoC Configuration

### Core Concept

All supported SoC lists and definitions are maintained in a single file: **`.github/scripts/socs_config.sh`**

This eliminates duplication and ensures consistency across all workflows and scripts.

### SoC Target Lists

The configuration defines several target lists for different purposes:

#### Platform-Specific Targets

- **`HW_TEST_TARGETS`** - Physical devices available in GitLab for hardware testing
- **`WOKWI_TEST_TARGETS`** - SoCs supported by Wokwi simulator
- **`QEMU_TEST_TARGETS`** - SoCs supported by QEMU emulator (currently disabled by default)
- **`BUILD_TEST_TARGETS`** - **Automatically computed** as the union of all test platform targets
  - This is the primary list used for builds and tests

**Note:** QEMU testing is disabled by default as Wokwi simulator can cover most of what QEMU does, with the added benefit of simulating more peripherals and providing a better testing experience.

#### IDF Version-Specific Targets

- **`IDF_V5_3_TARGETS`** - Supported by ESP-IDF v5.3
- **`IDF_V5_4_TARGETS`** - Supported by ESP-IDF v5.4
- **`IDF_V5_5_TARGETS`** - Supported by ESP-IDF v5.5
- **`IDF_COMPONENT_TARGETS`** - Default IDF targets (currently v5.5)

#### Special Lists

- **`ALL_SOCS`** - Complete list of all supported SoCs
- **`CORE_VARIANTS`** - SoC variants used for packaging/release flows (includes variant-specific entries)
- **`SKIP_LIB_BUILD_SOCS`** - SoCs without pre-built libraries (e.g., esp32c2, esp32c61)
- **`CORE_SOCS`** - Computed list of buildable SoCs (ALL_SOCS minus SKIP_LIB_BUILD_SOCS)

#### Exported CSV Helpers

For compatibility with scripts and workflows that consume CSV strings, the config exports:
- **`ALL_SOCS_CSV`**, **`CORE_SOCS_CSV`**, **`CORE_VARIANTS_CSV`**
- **`HW_TEST_TARGETS_CSV`**, **`WOKWI_TEST_TARGETS_CSV`**, **`QEMU_TEST_TARGETS_CSV`**
- **`BUILD_TEST_TARGETS_CSV`**, **`IDF_COMPONENT_TARGETS_CSV`**

### Helper Functions

The configuration provides utility functions for common operations:

#### SoC Properties
- **`get_arch`** - Returns the architecture (xtensa or riscv32) for a given SoC
- **`is_qemu_supported`** - Checks if a SoC is supported by QEMU emulator
- **`should_skip_lib_build`** - Checks if a SoC should skip library builds (no pre-built libs)

#### Array Conversion
- **`array_to_csv`** - Converts bash array to comma-separated string
- **`array_to_json`** - Converts bash array to JSON array string
- **`array_to_quoted_csv`** - Converts bash array to quoted comma-separated string

#### IDF Version Lookup
- **`get_targets_for_idf_version`** - Returns CSV list of targets supported by a specific IDF version

### Usage in Scripts

Scripts source the configuration file to access SoC lists and helper functions. The configuration provides arrays like `BUILD_TEST_TARGETS` that can be iterated over, and functions like `is_qemu_supported()` or `get_arch()` for runtime checks.

### Usage in Workflows

Workflows source the configuration in their setup steps and use the arrays to dynamically generate job matrices. This allows adding new SoCs without modifying workflow files - just update `socs_config.sh` and all workflows automatically include the new target.

---

## Workflows

### Compilation Tests (`push.yml`)

**Trigger:**
- Push to `master` or `release/*` branches
- Pull requests modifying:
  - `cores/**`
  - `libraries/**/*.cpp`
  - `libraries/**/*.c`
  - `libraries/**/*.h`
  - `libraries/**/*.ino`
  - `libraries/**/ci.yml`
  - `package/**`
  - `tools/get.*`
  - `package.json`
  - `platform.txt`
  - `programmers.txt`
  - `variants/**`
  - `.github/workflows/push.yml`
  - `.github/scripts/install-*`
  - `.github/scripts/on-push.sh`
  - `.github/scripts/sketch_utils.sh`
  - `.github/scripts/get_affected.py`
  - `.github/scripts/socs_config.sh`
  - `!*.md`
  - `!*.txt`
  - `!*.properties`
- Schedule: Every Sunday at 2:00 UTC (verbose logging)
- Manual workflow dispatch (with configurable log level)

**Purpose:** Build Arduino sketches across all SoC variants to catch compilation errors early

**Jobs:**

#### 1. `gen-chunks`
**Runs on:** Ubuntu-latest

**Steps:**
1. Install universal-ctags for symbol parsing
2. Checkout repository (fetch-depth: 2 for diff)
3. Get changed files using `tj-actions/changed-files`
4. **Check Official Variants** - Calls `check_official_variants.sh` with `BUILD_TEST_TARGETS`
5. **Set Chunks** - Runs `get_affected.py` to:
   - Parse C/C++ dependencies using ctags
   - Build forward/reverse dependency graphs
   - Determine affected sketches from changed files
   - Distribute sketches across chunks for parallel builds
6. Upload affected sketches and debug artifacts

**Outputs:**
- `should_run` - Whether workflow should proceed (based on variant check)
- `should_build` - Whether any sketches need building
- `chunk_count` - Number of parallel chunks needed
- `chunks` - JSON array of chunk indices

**SoC Config Usage:**
The script sources `socs_config.sh` and uses `BUILD_TEST_TARGETS` to determine which variants are considered official.

#### 2. `build-arduino-linux`
**Runs on:** Ubuntu-latest (matrix of chunks)

**Conditions:**
- `needs.gen-chunks.outputs.should_run == 'true'`
- `needs.gen-chunks.outputs.should_build == '1'`

**Matrix:** Chunks from gen-chunks job (up to 15 parallel jobs)

**Steps:**
1. Checkout repository
2. Setup Python 3.x
3. **Get libs cache** - Restores cached tools and libraries
   - Key: `libs-{os}-{arch}-{hash(package_esp32_index.template.json, tools/get.py)}`
   - Includes: esp32-arduino-libs, esptool, toolchains
4. Set log level (none/error/warn/info/debug/verbose)
5. Download affected sketches list
6. **Build sketches** - Runs `on-push.sh` with chunk parameters
7. Upload compile results JSON

**SoC Config Usage:**
The build script sources `socs_config.sh` and iterates through `CORE_SOCS` (ALL_SOCS minus SKIP_LIB_BUILD_SOCS), building sketches for each supported SoC.

**Environment Variables:**
- `MAX_CHUNKS=15` - Maximum parallel build jobs

**Key Features:**
- **Intelligent chunking**: Only rebuilds affected sketches
- **Parallel execution**: Up to 15 simultaneous builds
- **Dependency tracking**: Uses ctags for accurate change detection
- **Size reports**: Generates JSON with sketch sizes for comparison
- **Caching**: Libraries and tools cached between runs

#### 3. `build-arduino-win-mac`
**Runs on:** Windows-latest, macOS-latest

**Conditions:**
- `needs.gen-chunks.outputs.should_run == 'true'`
- `needs.gen-chunks.outputs.recompile_preset == '1'`

**Purpose:** Verify cross-platform compilation compatibility

**Steps:**
1. Checkout repository
2. Setup Python 3.x
3. Install yq (Windows only - macOS/Linux have it by default)
4. **Build preset sketches** - Runs `on-push.sh` without parameters
   - Builds hardcoded list of representative sketches
   - Tests WiFiClientSecure, BLE Server, Camera, Insights examples

**Why Preset Sketches:**
- Windows/macOS builds are slower than Linux
- Full matrix would take too long
- Preset ensures cross-platform compatibility for critical examples

#### 4. `save-master-artifacts`
**Runs on:** Ubuntu-latest

**Conditions:**
- Only on push to `master` branch
- After `build-arduino-linux` completes

**Purpose:** Archive compile results for historical tracking

**Steps:**
1. Checkout gh-pages branch
2. Download all compile result artifacts
3. Commit and push to gh-pages branch
4. Enables size comparison with future PRs

#### 5. `upload-pr-number`
**Runs on:** Ubuntu-latest

**Conditions:**
- Only on pull requests (not release branches)

**Purpose:** Store PR number for status reporting in later workflows

**Concurrency:**
- Group: `build-{pr_number or ref}`
- Cancel in progress: Yes (saves resources on force-pushes)

### Runtime Tests

The runtime test system is a multi-stage pipeline that builds test sketches once and then executes them across multiple platforms (QEMU emulator, physical hardware, and Wokwi simulator). The system uses GitHub Actions for orchestration and building, while actual hardware tests run in GitLab CI for access to the internal test lab.

**Trigger:**
- Pull requests that touch tests, cores, libraries, or CI scripts/workflows
- Daily schedule (00:00 UTC)
- Manual workflow dispatch

**Purpose:**
Validate runtime behavior across all supported SoCs on three different platforms, catching issues that compilation testing cannot detect.

---

#### Test Flow Architecture

The system consists of four GitHub Actions workflows that execute in sequence, plus GitLab CI for hardware testing:

```
tests.yml (Orchestrator - GitHub Actions)
    │
    ├─→ Calls: tests_build.yml
    │       Compiles test binaries for all SoCs
    │       Uploads artifacts to GitHub
    │
    └─→ Calls: tests_qemu.yml (disabled by default)
            QEMU tests currently disabled
            Wokwi covers most QEMU functionality with better peripheral support

        ↓ (workflow_run trigger on completion)

tests_hw_wokwi.yml (GitHub Actions)
    │   Requires access to the repository secrets
    │   Downloads test matrix and configuration
    │
    ├─→ Triggers: GitLab CI Pipeline
    │       Downloads test binaries from GitHub
    │       Dynamically generates jobs per chip/type
    │       Runs on physical hardware with runner tags
    │       Hardware tests use internal test lab
    │
    └─→ Triggers: Wokwi Tests
            Runs in GitHub Actions with Wokwi CLI
            Simulates hardware behavior

        ↓ (workflow_run trigger on completion)

tests_results.yml (GitHub Actions)
    Aggregates results from QEMU, hardware, and Wokwi
    Identifies missed tests
    Posts comprehensive report to PR
```

**Why workflow_run?**
- `tests_hw_wokwi.yml` needs elevated permissions not available in PR context
- `tests_results.yml` must wait for all platforms to complete
- Separates permissions contexts for security

---

#### Phase 1: Orchestration (`tests.yml`)

The main workflow coordinates the initial testing stages using GitHub Actions workflow_call feature.

**Key Responsibilities:**

1. **Event Handling**:
   - Uploads event file artifact for downstream workflows
   - Preserves PR context and commit information

2. **Matrix Generation**:
   - Sources `socs_config.sh` to get platform-specific target lists
   - Creates matrices for builds (all `BUILD_TEST_TARGETS`)
   - Determines test types to run based on PR labels and event type
   - Generates `test_matrix.json` with complete configuration
   - Validation tests always run; performance tests require `perf_test` label or scheduled run

3. **Workflow Orchestration via workflow_call**:
   - **Calls `tests_build.yml`**: Matrix of chip × type combinations
   - **Calls `tests_qemu.yml`**: QEMU tests (disabled by default - Wokwi provides better coverage)
   - Runs in parallel for faster feedback
   - Both have access to PR context and can download artifacts

4. **Triggers Downstream**:
   - Completion triggers `tests_hw_wokwi.yml` via workflow_run
   - Uploads artifacts that downstream workflows need

**Test Type Logic:**
- **PRs**: Only validation tests (fast feedback)
- **Scheduled runs**: Validation + performance builds (hardware-only execution)
- **PR with `perf_test` label**: Validation + performance builds (hardware-only execution)

**Platform Gates:**
- **Hardware tests** require `GITLAB_ACCESS_TOKEN`; on PRs they also require the `hil_test` label
- **Wokwi tests** require `WOKWI_CLI_TOKEN`

**Concurrency**: Cancels in-progress runs for same PR to save resources

---

#### Phase 2: Build (`tests_build.yml`)

Compiles test sketches once for reuse across all test platforms.

**Build Strategy:**
- Receives `chip` and `type` inputs from orchestrator
- Checks cache first - if binaries already exist from previous run, skip building
- Installs Arduino CLI and ESP32 core
- Compiles all test sketches for the specified chip and test type
- Stores binaries in standardized directory structure

**Output Artifacts:**
Each build produces binaries organized by chip and test type, including:
- Compiled `.bin` files (flashable binaries)
- `.elf` files (for debugging)
- Merged binaries (bootloader + partition table + app)
- `sdkconfig` (ESP-IDF configuration used)
- `ci.yml` (test requirements and configurations)

**Caching Strategy:**
Build artifacts are cached with a key based on PR number and commit SHA. This allows:
- Fast rebuilds when retrying failed tests
- Skipping builds when only test code changes
- Reduced CI time and resource usage

---

#### Phase 3A: QEMU Testing (`tests_qemu.yml`)

Runs tests in QEMU emulator for quick feedback without hardware.

**Why QEMU:**
- Fast execution (seconds per test)
- No hardware bottlenecks
- Deterministic environment
- Good for basic functionality validation

**Process:**
1. Downloads QEMU from espressif/qemu releases (cached)
2. Fetches test binaries from build phase
3. Downloads and installs pytest-embedded framework
4. Uses `socs_config.sh` helper functions to determine correct QEMU architecture
5. Runs pytest with QEMU-specific configurations
6. Generates JUnit XML reports
7. Caches results to avoid re-running on retry

**Architecture Detection:**
The test runner calls `get_arch()` to dynamically determine whether to use xtensa or riscv32 QEMU binary, making it easy to add new SoCs without modifying the test logic.

---

#### Phase 3: Hardware & Wokwi Testing (`tests_hw_wokwi.yml`)

**Trigger**: `workflow_run` event when `tests.yml` completes (success or failure)

**Why workflow_run?**
- Requires elevated permissions (`actions: read`, `statuses: write`)
- PR workflows run with restricted permissions for security
- Cannot access organization secrets in PR context
- workflow_run provides isolated permission boundary

**Architecture**: Two separate testing paths

##### Part A: Hardware Tests (GitLab CI)

**Why GitLab CI:**
- Access to internal test lab with physical ESP32 hardware
- Secure credential management for test infrastructure
- Runner tags for specific hardware assignment
- Better resource scheduling for physical devices

**Workflow Steps:**

1. **GitHub Actions Stage** (`tests_hw_wokwi.yml`):
   - Downloads test matrix from `tests.yml` artifacts
   - Extracts chip and type configurations
   - Reports pending status to commit
   - **Triggers GitLab CI pipeline** via webhook

2. **GitLab CI Stage** (`.gitlab/workflows/hardware_tests_dynamic.yml`):

   **Job 1: `generate-hw-tests`**
   - Downloads test binaries from GitHub artifacts
   - Runs `gen_hw_jobs.py` to dynamically create child pipeline
   - Groups tests by chip and required runner tags
   - Generates `child-hw-jobs.yml` with job definitions

   **Job 2: `trigger-hw-tests`**
   - Triggers child pipeline with generated jobs
   - Each job runs on hardware with matching tags (e.g., `esp32s3` runner)
   - Uses template from `.gitlab/workflows/hw_test_template.yml`

   **Job Template Execution** (per chip/type):
   - Runs on GitLab runner with physical hardware attached
   - Downloads test binaries for specific chip
   - Flashes binary to device via esptool
   - Executes `tests_run.sh` with hardware platform flag
   - Runs pytest with `--embedded-services esp,arduino`
   - Provides WiFi credentials from runner environment
   - Monitors serial output for test results
   - Generates JUnit XML reports

   **Job 3: `collect-hw-results`**
   - Waits for all hardware jobs to complete
   - Aggregates JUnit XML results
   - Uploads consolidated artifacts

3. **GitHub Actions Completion**:
   - Monitors GitLab pipeline status
   - Reports final status to commit
   - Uploads results as GitHub artifacts

**Hardware Infrastructure:**
- Physical devices for all `HW_TEST_TARGETS` SoCs
- Runner tags match chip names for job assignment
- 5-hour timeout for long-running tests
- WiFi network access for connectivity tests

##### Part B: Wokwi Tests (GitHub Actions)

**Infrastructure:**
- Runs in GitHub Actions with Wokwi CLI token
- SoCs in `WOKWI_TEST_TARGETS` supported
- No physical hardware required

**Process:**
1. Downloads test binaries from `tests.yml` artifacts
2. Installs Wokwi CLI with organization token
3. Runs pytest with `--embedded-services arduino,wokwi`
4. Uses sketch's `diagram.{chip}.json` for hardware simulation
5. Simulates peripherals, sensors, displays
6. Generates JUnit XML reports

**Advantages:**
- More peripherals than QEMU (sensors, displays, buttons)
- No hardware availability bottlenecks
- Faster than physical hardware
- Deterministic behavior for debugging

---

#### Phase 4: Results Aggregation (`tests_results.yml`)

**Trigger**: `workflow_run` event when `tests_hw_wokwi.yml` completes

**Purpose:**
Aggregate results from all three test platforms (QEMU, hardware, Wokwi), identify any missed tests, and generate comprehensive report.

**Why After tests_hw_wokwi.yml:**
- Must wait for hardware tests to complete (longest running)
- Needs results from all platforms for complete picture
- Can access all artifacts once everything finishes

**Process:**

1. **Artifact Collection**:
   - Downloads artifacts from originating `tests.yml` run
   - Downloads artifacts from `tests_hw_wokwi.yml` run
   - Extracts test matrix configuration to know what was expected
   - Collects JUnit XML from:
     - QEMU tests (if enabled - currently disabled by default)
     - Hardware tests (from GitLab via tests_hw_wokwi.yml)
     - Wokwi tests (from tests_hw_wokwi.yml)

2. **Analysis Phase**:
   - Parses all JUnit XML files
   - Cross-references with expected test matrix
   - **Identifies missed tests**: Tests that should have run but didn't
   - Categorizes results by:
     - Platform (QEMU/hardware/Wokwi)
     - SoC variant
     - Test type (validation/performance)
   - Calculates pass/fail/skip/missing counts
   - Identifies patterns in failures

3. **Report Generation**:
   - Creates markdown summary table
   - Shows results matrix: platforms × SoCs × test types
   - **Highlights missed tests** with reasons (build failure, platform unavailable, etc.)
   - Lists failed tests with error excerpts
   - Includes links to:
     - Full logs in GitHub Actions
     - GitLab CI pipeline for hardware tests
     - Artifact downloads
   - Compares with previous runs (if available)

4. **PR Communication**:
   - Posts comprehensive comment to pull request
   - Updates commit status
   - Provides actionable information for developers

**Report Sections:**
- **Summary**: Overall statistics across all platforms
- **Per-Platform Results**: Breakdown by hardware/Wokwi (and QEMU if enabled)
- **Failed Tests**: Detailed information with logs
- **Missed Tests**: What didn't run and why
- **Performance Metrics**: If performance tests ran
- **Comparison**: Changes from previous test run

**Missing Test Detection:**
Critical feature that ensures no tests are silently skipped due to infrastructure issues. If hardware is unavailable or binaries fail to download, the report explicitly calls this out rather than showing false-positive "all passed."

---

#### Test Organization

Tests are organized in the repository under `tests/` directory:

- `tests/validation/` - Basic functionality tests (always run)
- `tests/performance/` - Performance benchmarks (conditional)

Each test directory contains:
- `{test_name}.ino` - Arduino sketch with test code
- `test_{test_name}.py` - pytest test file
- `ci.yml` - Test requirements and configuration
- `diagram.{chip}.json` - Optional Wokwi hardware diagram

**Test Configuration (`ci.yml`):**
Defines test requirements and platform support:
- `targets: {chip: true/false}` - Which SoCs support this test
- `platforms: {platform: true/false}` - Which platforms can run it
- `tags: [tag1, tag2]` - Runner tags or infrastructure constraints (hardware tests)
- `multi_device: {device0: sketchA, device1: sketchB}` - Multi-DUT test mapping
- `requires: [CONFIG_X=y]` - Required sdkconfig settings
- `requires_or: [CONFIG_A=y, CONFIG_B=y]` - Alternative requirements
- `libs: [Library1, Library2]` - Required libraries

Example:
```yaml
targets:
  esp32: true
  esp32c3: true
  esp32c6: false
  esp32h2: false
  esp32p4: false
  esp32s2: false
  esp32s3: true
platforms:
  qemu: false
  wokwi: true
  hardware: true
requires:
  - CONFIG_SOC_WIFI_SUPPORTED=y
```

### Multi-Device (Multi-DUT) Tests

Some tests require two physical devices (e.g., BLE server/client, WiFi AP/client). These are defined using the `multi_device` field in `ci.yml`, where each entry points to a sketch directory for that device.

**How it works:**
- `tests_build.sh` builds each device sketch separately and stores artifacts under `~/.arduino/tests/<target>/<test>_<sketch>/build.tmp`.
- `tests_run.sh` assembles multiple build directories and ports using `|` separators, then runs pytest with `--count` and `--embedded-services` for each DUT.
- Only **2 devices** are supported currently.
- **Hardware only**: multi-device tests are blocked for Wokwi and QEMU.
- Set ports via `ESPPORT1` and `ESPPORT2` (single-device tests can use `ESPPORT`).

**Test Execution:**
The test runner (`tests_run.sh`) automatically:
- Checks requirements against built sdkconfig
- Skips tests that don't meet requirements
- Installs required libraries
- Configures platform-specific parameters
- Retries failed tests once
- Generates JUnit XML reports

---

#### Platform Comparison

| Platform | Speed | Coverage | Hardware | Use Case |
|----------|-------|----------|----------|----------|
| **QEMU** | Fastest | Basic | Emulated | Quick validation |
| **Wokwi** | Fast | Wide | Simulated | Peripheral testing |
| **Hardware** | Slower | Complete | Real | Final validation |

**Combined Strategy:**
All three platforms complement each other:
- QEMU provides quick feedback for basic issues
- Wokwi tests peripheral interactions without hardware constraints
- Physical hardware validates real-world behavior

This multi-platform approach ensures comprehensive testing while maintaining reasonable CI execution times.

### IDF Component Build (`build_component.yml`)

**Trigger:**
- Push to `master` or `release/*` branches
- Pull requests modifying:
  - `cores/**`
  - `libraries/**/*.cpp`
  - `libraries/**/*.c`
  - `libraries/**/*.h`
  - `idf_component_examples/**`
  - `idf_component.yml`
  - `Kconfig.projbuild`
  - `CMakeLists.txt`
  - `variants/**`
  - `.github/workflows/build_component.yml`
  - `.github/scripts/check-cmakelists.sh`
  - `.github/scripts/on-push-idf.sh`
  - `.github/scripts/sketch_utils.sh`
  - `.github/scripts/get_affected.py`
  - `.github/scripts/socs_config.sh`
  - `!*.md`
  - `!*.txt`
  - `!*.properties`
- Manual workflow dispatch with customizable IDF versions/targets

**Purpose:** Ensure Arduino core works correctly as an ESP-IDF component

**Inputs (Manual Dispatch):**
- `idf_ver`: Comma-separated IDF branches (default: "release-v5.5")
- `idf_targets`: Comma-separated targets (default: empty = use version-specific defaults)

**Default Matrix (PRs/Pushes):**
- Versions: `release-v5.3`, `release-v5.4`, `release-v5.5`
- Targets: derived from `get_targets_for_idf_version()` unless overridden

**Jobs:**

##### 1. `cmake-check`
**Runs on:** Ubuntu-latest

**Purpose:** Validate CMakeLists.txt completeness

**Steps:**
1. Checkout repository
2. **Run check script**:
   ```bash
   bash ./.github/scripts/check-cmakelists.sh
   ```

**Check Process:**
The script compares source files found in the repository against files listed in `CMakeLists.txt`. It uses cmake's trace feature to extract the source list, then diffs against actual files. Any mismatch causes the check to fail.

**Why Important:**
ESP-IDF's CMake-based build system requires explicit source file listing. Missing files won't compile, extra files indicate stale CMakeLists.txt. This check prevents both issues.

##### 2. `check-examples`
**Runs on:** Ubuntu-latest

**Purpose:** Ensure `idf_component_examples/` matches `idf_component.yml` declarations

**Steps:**
1. Checkout repository
2. Compare folders in `idf_component_examples/` with `idf_component.yml` entries

##### 3. `set-matrix`
**Runs on:** Ubuntu-latest

**Purpose:** Generate IDF version × target matrix

**Steps:**
1. Install universal-ctags
2. Checkout repository (fetch-depth: 2)
3. Get changed files
4. **Check official variants** - Calls `check_official_variants.sh` with `IDF_COMPONENT_TARGETS`
5. **Get affected examples** - Runs `get_affected.py --component`
6. Upload affected examples and debug artifacts
7. **Generate matrix combinations**:

**Matrix Generation Process:**
The script sources `socs_config.sh` and creates combinations of IDF versions and targets. It uses the centralized `get_targets_for_idf_version()` function to get version-specific target lists unless manual inputs override them. This generates a JSON matrix with entries like `{idf_ver: "release-v5.5", idf_target: "esp32c3"}`.

**Outputs:**
- `should_run` - Based on official variant check
- `matrix` - JSON with IDF version × target combinations
- `should_build` - Based on affected examples

The resulting matrix includes all combinations of supported IDF versions and their respective targets. For example, v5.3 might have 8 targets while v5.5 has 10 (including esp32c5 and esp32c61).

##### 4. `build-esp-idf-component`
**Runs on:** Ubuntu-latest

**Conditions:**
- `needs.set-matrix.outputs.should_run == 'true'`
- `needs.set-matrix.outputs.should_build == '1'`

**Container:** `espressif/idf:${{ matrix.idf_ver }}`

**Matrix:** From `set-matrix` output

**Steps:**
1. **Checkout as component**:
   - Path: `components/arduino-esp32`
   - With submodules: recursive

2. **Setup jq and yq**:
   - jq: For JSON parsing
   - yq: For YAML parsing (ci.yml files)

3. **Download affected examples**

4. **Build examples**:
   ```bash
   export IDF_TARGET=${{ matrix.idf_target }}
   ./components/arduino-esp32/.github/scripts/on-push-idf.sh affected_examples.txt
   ```

**Build Process:**
For each affected example, the script checks if the current target is supported (via ci.yml), applies target-specific sdkconfig defaults if available, then uses ESP-IDF's standard build commands (`idf.py set-target` and `idf.py build`).

5. **Upload sdkconfig files** (on failure):
   - Artifact: `sdkconfig-{idf_ver}-{idf_target}`
   - Helps debug configuration issues

**Why Multiple IDF Versions:**
- Ensures compatibility across ESP-IDF releases
- Catches breaking changes early
- Supports users on different IDF versions

**Concurrency:**
- Group: `build-component-{pr_number or ref}`
- Cancel in progress: Yes

### Board Validation (`boards.yml`)

**Trigger:**
- Pull requests modifying:
  - `boards.txt`
  - `libraries/ESP32/examples/CI/CIBoardsTest/CIBoardsTest.ino`
  - `.github/workflows/boards.yml`

**Purpose:** Validate new or modified board definitions

**Jobs:**

##### 1. `find-boards`
**Runs on:** Ubuntu-latest

**Purpose:** Identify boards that changed in the PR

**Steps:**
1. Checkout repository
2. **Get board name** from PR changes:
   ```bash
   bash .github/scripts/find_new_boards.sh ${{ github.repository }} ${{ github.base_ref }}
   ```

**Detection Logic:**
The script fetches `boards.txt` from both the base branch and PR branch, extracts board names from each, then compares to find additions and modifications. Only boards that are new or have changed definitions are flagged for testing.

**Outputs:**
- `fqbns` - JSON array of board FQBNs to test

##### 2. `test-boards`
**Runs on:** Ubuntu-latest

**Conditions:** Only if `find-boards.outputs.fqbns != ''`

**Matrix:** FQBNs from find-boards job

**Steps:**
1. Checkout repository

2. **Validate board definition** using `validate_board.sh` script

**Validation Checks:**
- Required properties exist:
  - `{board}.name`
  - `{board}.upload.tool`
  - `{board}.upload.maximum_size`
  - `{board}.build.mcu`
  - `{board}.build.core`
  - `{board}.build.variant`
  - `{board}.build.board`
- Build options are valid
- Upload configurations are correct
- Memory sizes are reasonable
- Variant directory exists

3. **Get libs cache** (same as other workflows)

4. **Compile test sketch**:
   - Uses `P-R-O-C-H-Y/compile-sketches` action
   - Sketch: `libraries/ESP32/examples/CI/CIBoardsTest/CIBoardsTest.ino`
   - FQBN: `${{ matrix.fqbn }}`
   - Warnings: all

**Test Sketch Features:**
- Includes common libraries (WiFi, BLE, etc.)
- Tests board-specific features
- Validates pin definitions
- Checks variant configuration

### Library Tests (`lib.yml`)

**Trigger:**
- Pull requests with `lib_test` label
- Schedule: Weekly on Sunday at 4:00 AM UTC

**Purpose:** Test Arduino core compatibility with popular external libraries

**Jobs:**

##### 1. `gen-matrix`
**Runs on:** Ubuntu-latest

**Purpose:** Generate matrix of targets from centralized config

**Steps:**
1. Checkout repository
2. **Generate matrix** by sourcing `socs_config.sh` and creating JSON entries for each target in `BUILD_TEST_TARGETS`, pairing each SoC with its corresponding FQBN

**Outputs:**
- `matrix` - JSON with target and FQBN for each SoC

**Why All Targets:** External libraries should work on all supported SoCs

##### 2. `compile-sketch`
**Runs on:** Ubuntu-latest

**Matrix:** From gen-matrix output (one job per SoC)

**Steps:**
1. Checkout repository

2. **Compile external library sketches**:
   - Uses `P-R-O-C-H-Y/compile-sketches` action
   - Target: `${{ matrix.target }}`
   - FQBN: `${{ matrix.fqbn }}`
   - Library list: `.github/workflows/lib.json`
   - Enables delta reports (size comparison)
   - Enables warnings report

**lib.json Contents:**
The file lists popular Arduino libraries with their names, which examples to test (regex pattern or specific list), and version constraints. This ensures compatibility testing with commonly-used external libraries.

3. **Upload artifacts**:
   - Name: `libraries-report-{target}`
   - Contains: Compile results and size data

##### 3. `report-to-file`
**Runs on:** Ubuntu-latest

**Conditions:**
- Only on scheduled runs (not PRs)
- After compile-sketch completes

**Purpose:** Archive library test results to gh-pages

**Steps:**
1. Checkout gh-pages branch
2. Download all library report artifacts
3. **Generate report**:
   - Uses `P-R-O-C-H-Y/report-size-deltas` action
   - Input: All sketch reports
   - Output: `LIBRARIES_TEST.md`
4. Append GitHub Action link
5. Commit and push to gh-pages

**Report Contents:**
- Library compatibility status
- Sketch sizes per SoC
- Size changes over time
- Compilation warnings/errors

##### 4. `upload-pr-number`
**Conditions:** Only on PRs with `lib_test` label

**Purpose:** Store PR number for status updates

**Concurrency:**
- Group: `libs-{pr_number or ref}`
- Cancel in progress: Yes

### All Boards Test (`allboards.yml`)

**Trigger:** Repository dispatch event with type `test-boards`

**Purpose:** Test all board definitions (not just new ones)

**Use Case:** Manually triggered for comprehensive validation before release

**Jobs:**

##### 1. `find-boards`
**Steps:**
- Lists all boards from `boards.txt`
- Uses `find_all_boards.sh` script
- Skips boards in `SKIP_LIB_BUILD_SOCS`

##### 2. `setup-chunks`
**Purpose:** Divide boards into chunks for parallel testing
- Maximum 15 chunks
- Distributes boards evenly

##### 3. `test-boards`
**Matrix:** Chunks from setup-chunks
- Validates each board
- Compiles test sketch
- Same process as `boards.yml`

### Documentation (`docs_build.yml`, `docs_deploy.yml`)

**Trigger:**
- `docs_build.yml`:
  - Push to `master` or `release/v2.x` (docs or workflow file changes)
  - Pull requests modifying `docs/**` or the workflow file
- `docs_deploy.yml`:
  - `workflow_run` after **ESP32 Arduino Release**
  - Push to `master` or `release/v2.x` when docs change

**Purpose:** Build and deploy Sphinx documentation to the production docs server

**Jobs:**

##### `docs_build.yml`
1. Setup Python with Sphinx
2. Install documentation dependencies
3. Build HTML from RST files
4. Check for warnings/errors
5. Upload built docs as an artifact

##### `docs_deploy.yml`
**Conditions:** Only runs in `espressif/arduino-esp32`

1. Build docs
2. Deploy to production server using `DOCS_*` secrets
3. Publishes to the configured docs URL

### Release (`release.yml`)

**Trigger:**
- GitHub release event (`published`)

**Purpose:** Create GitHub releases and publish packages

**Jobs:**

##### 1. `build`
**Steps:**
1. Checkout release target commit
2. Run `on-release.sh` to:
   - Build ZIP/XZ core package
   - Download and repackage libs into per-SoC archives
   - Update `package_esp32_*` JSONs (including CN variants)
   - Stage hosted binaries extracted from libs
3. Upload `build/` output and `hosted/` artifacts

##### 2. `test-package`
**Runs on:** Windows, macOS, Ubuntu

**Steps:**
- Download `build/` artifacts
- Run `test-package-json.sh` to validate package installation

##### 3. `upload-and-finalize`
**Steps:**
- Upload package JSONs to GitHub Releases and GitHub Pages
- Use `upload-release-assets.sh` to update version commit and retag if needed

##### 4. `upload-hosted-binaries`
**Steps:**
- Copy new `esp-hosted` binaries to `gh-pages/hosted`
- Commit and push if new binaries are present

**Optional S3 Upload:**
- `ENABLE_S3=true` with `S3_BUCKET_*` vars uploads per-SoC libs ZIPs to S3
- Otherwise libs are attached to the GitHub release

### Size Reporting (`publishsizes.yml`, `publishsizes-2.x.yml`)

**Trigger:**
- `publishsizes.yml`: `workflow_run` after **Compilation Tests** (PRs), plus manual dispatch
- `publishsizes-2.x.yml`: manual dispatch (master vs `v2.x` comparison)

**Purpose:** Post compile size comparison comment to PR

**Jobs:**
1. Download compile results from master (gh-pages)
2. Download compile results from PR
3. Compare sizes
4. Generate markdown table
5. Post comment to PR showing:
   - Size changes per sketch
   - Percentage changes
   - Memory usage trends

### Pre-commit (`pre-commit.yml`, `pre-commit-status.yml`)

**Trigger:**
- Push to `master`
- Pull requests (opened/reopened/synchronize/labeled)
- Manual workflow dispatch

**Purpose:** Run code quality checks

**pre-commit.yml:**
- Runs pre-commit hooks
- Checks code formatting
- Validates file structure
- Ensures style guidelines
- PRs only run when labeled `Status: Pending Merge` or `Re-trigger Pre-commit Hooks`

**pre-commit-status.yml:**
- Reports pre-commit status to PR
- Can block merge if checks fail

### CodeQL Security (`codeql.yml`)

**Trigger:**
- Manual workflow dispatch
- Push to `master`
- Pull requests that touch source or workflow files (`**/*.{c,cpp,h,ino,py}` and `.github/workflows/*`)

**Purpose:** Security vulnerability scanning

**Steps:**
1. Initialize CodeQL
2. Autobuild C/C++ code
3. Perform security analysis
4. Upload results to Security tab

---

### Build Python Tools (`build_py_tools.yml`)

**Trigger:**
- Pull requests modifying:
  - `.github/workflows/build_py_tools.yml`
  - `tools/get.py`
  - `tools/espota.py`
  - `tools/gen_esp32part.py`
  - `tools/gen_insights_package.py`
  - `tools/bin_signing.py`

**Purpose:**
Automatically build cross-platform executables for Python tools when they're modified in a PR. This ensures the pre-compiled binaries stay in sync with source code changes.

**Important:** PRs that modify Python tools **must** use branches on `espressif/arduino-esp32` repository (not from forks). This is required because:
- The CI needs to sign binaries using organization secrets (Azure Key Vault)
- Compiled binaries must be pushed back to the PR branch
- Fork-based PRs don't have write access or secret access for security reasons

The workflow will fail with a helpful message if attempted from a fork.

**Permissions:**
- `contents: write` - For pushing built binaries
- `pull-requests: read` - For accessing PR information

**Jobs:**

#### 1. `find-changed-tools`
**Runs on:** Ubuntu-latest

**Purpose:** Detect which Python tools have been modified

**Steps:**
1. **Checkout repository**:
   - Fetch depth: 2
   - Checks out PR branch
   - Fails with helpful message if using a fork (requires branch in main repo)

2. **Verify Python Tools Changed**:
   - Uses `tj-actions/changed-files@v46.0.1`
   - Checks for changes in specific tool files
   - Outputs: `any_changed`, `all_changed_files`

3. **List changed files** for visibility in logs

**Outputs:**
- `any_changed` - Boolean indicating if any tools changed
- `all_changed_files` - Space-separated list of changed tool paths

#### 2. `build-pytools-binaries`
**Runs on:** Multi-platform matrix

**Conditions:** Only if `find-changed-tools.outputs.any_changed == 'true'`

**Matrix Strategy:**
- **Platforms:**
  - `windows-latest` - Windows 64-bit
  - `macos-latest` - macOS (Intel/Apple Silicon universal)
  - `ubuntu-latest` - Linux AMD64
  - `ubuntu-24.04-arm` - Linux ARM64

**Platform-Specific Configuration:**
```
windows-latest:   TARGET: win64,      EXTEN: .exe,  SEPARATOR: ;
macos-latest:     TARGET: macos,      EXTEN: (none), SEPARATOR: :
ubuntu-latest:    TARGET: linux-amd64, EXTEN: (none), SEPARATOR: :
ubuntu-24.04-arm: TARGET: arm,        EXTEN: (none), SEPARATOR: :
```

**Environment:**
- `DISTPATH: pytools-{TARGET}` - Output directory for built binaries
- `PIP_EXTRA_INDEX_URL: https://dl.espressif.com/pypi` - ESP-specific packages

**Steps:**

1. **List changed tools**:
   - Parses `all_changed_files` from previous job
   - Extracts tool names (removes `tools/` prefix and `.py` extension)
   - Sets `CHANGED_TOOLS` environment variable

2. **Checkout repository**:
   - Uses `TOOLS_UPLOAD_PAT` token (for push permissions)
   - Checks out PR branch

3. **Set up Python 3.8**:
   - Uses older Python for maximum compatibility
   - Ensures binaries work on older systems

4. **Install dependencies**:
   - Upgrades pip
   - Installs: `pyinstaller`, `requests`, `cryptography`

5. **Build with PyInstaller**:
   - For each changed tool:
     - Runs: `pyinstaller --distpath ./{DISTPATH} -F --icon=.github/pytools/espressif.ico tools/{tool}.py`
     - `-F`: Single-file executable
     - `--icon`: Espressif icon for branding
   - Outputs to platform-specific directory

6. **Sign binaries** (Windows only):
   - Uses `espressif/release-sign` action
   - Azure Key Vault signing for Windows executables
   - Secrets required:
     - `AZURE_CLIENT_ID`
     - `AZURE_CLIENT_SECRET`
     - `AZURE_TENANT_ID`
     - `AZURE_KEYVAULT_URI`
     - `AZURE_KEYVAULT_CERT_NAME`
   - Ensures Windows SmartScreen doesn't flag binaries

7. **Test binaries**:
   - For each changed tool:
     - Runs: `./{DISTPATH}/{tool}{EXTEN} -h`
   - Verifies binary executes and shows help
   - Catches obvious build failures

8. **Push binary to tools** (Windows only):
   - Copies Windows executable to `tools/{tool}.exe`
   - Calls `upload_py_tools.sh` script
   - Commits and pushes to PR branch
   - Only Windows binaries are committed (Linux/macOS built on user machines)

**Windows Signing Notes:**
- Signing runs only on the Windows matrix job and uses Azure Key Vault credentials.
- Signed `.exe` artifacts are uploaded and the Windows binaries are committed back to the PR branch.

9. **Archive artifact**:
   - Uploads built binaries as GitHub Actions artifacts
   - Artifact name: `pytools-{TARGET}`
   - Available for download and testing

**Why Only Windows Binaries Committed:**
- Windows users typically don't have Python installed
- Linux/macOS users often have Python available
- Reduces repository size
- Windows binaries require signing (most complex)

**Fail-Fast:** False - All platforms build independently

**Use Case:**
When a developer modifies `tools/espota.py` and creates a PR, this workflow automatically:
1. Detects the change
2. Builds `espota.exe` for Windows (x64), macOS, and Linux (amd64, arm64)
3. Signs the Windows binary
4. Tests all binaries
5. Commits `espota.exe` to the PR
6. Uploads all platform binaries as artifacts

This ensures the pre-compiled tools stay in sync with source code, and Windows users get properly signed executables that won't trigger security warnings.

### Backlog Bot (`backlog-bot.yml`)

**Trigger:**
- Manual workflow dispatch (supports `dry-run` input)

**Purpose:** Manage stale issues and keep the backlog tidy

**Behavior (backlog-cleanup.js):**
- Processes **issues only** (pull requests are skipped)
- Skips issues with exempt labels
- Adds **Move to Discussion** label to `Type: Question`
- Closes issues with `Status: Awaiting Response` or no assignees after 90+ days of inactivity
- Posts a friendly reminder on stale assigned issues (at most once every 7 days)

**Permissions:** `issues: write`, `discussions: write`, `contents: read`

---

## Scripts

### Core Scripts

#### `socs_config.sh`
**Purpose:** Central configuration for all SoC definitions

**Contains:**
- All SoC target lists
- Helper functions for SoC operations
- IDF version mappings
- QEMU architecture mappings

**Usage:** Sourced by all other scripts and workflows

#### `check_official_variants.sh`
**Purpose:** Determine if workflow should run based on variant changes

**Usage:**
```bash
bash .github/scripts/check_official_variants.sh \
  "${{ github.event_name }}" \
  "BUILD_TEST_TARGETS" \
  ${{ steps.changed-files.outputs.all_changed_files }}
```

**Logic:**
- Always run if not a PR
- Check if any official variant files changed
- Check if any non-variant files changed
- Skip only if PR changes non-official variants exclusively

### Build Scripts

#### `on-push.sh`
**Purpose:** Build sketches for push workflow

**Features:**
- Builds all targets from `BUILD_TEST_TARGETS`
- Supports chunked parallel builds
- Generates compile size reports
- Uses `sketch_utils.sh` for actual compilation

#### `sketch_utils.sh`
**Purpose:** Core Arduino sketch compilation utilities

**Functions:**
- `build_sketch` - Build a single sketch
- `chunk_build` - Build sketches in chunks
- `count` - Count sketches for chunking
- `check_requirements` - Validate sketch requirements
- `install_libs` - Install library dependencies

### Test Scripts

#### `tests_matrix.sh`
**Purpose:** Generate test matrices for workflows

**Uses:** Sources `socs_config.sh` to create JSON matrices of targets and types

#### `tests_build.sh`
**Purpose:** Build test sketches

**Features:**
- Sources `socs_config.sh` for targets
- Builds tests with Arduino CLI
- Supports multiple test types

#### `tests_run.sh`
**Purpose:** Run compiled tests on different platforms

**Platforms:**
- **hardware** - Physical devices
- **wokwi** - Wokwi simulator
- **qemu** - QEMU emulator

**Features:**
- Auto-detects QEMU architecture using `get_arch()`
- Supports retry on failure
- Generates JUnit XML reports

### Board Scripts

#### `find_all_boards.sh`
**Purpose:** List all board FQBNs for testing

**Filter:** Uses `should_skip_lib_build()` to exclude boards without pre-built libs

#### `find_new_boards.sh`
**Purpose:** Detect boards modified in PR

#### `validate_board.sh`
**Purpose:** Validate board definition completeness

### Utility Scripts

#### `get_affected.py`
**Purpose:** Intelligent affected files detection

**Features:**
- Analyzes dependencies between files
- Uses ctags for symbol mapping
- Determines which sketches need rebuilding
- Supports both Arduino and IDF component modes

#### `install-*.sh`
**Purpose:** Install Arduino CLI, IDE, and core

#### Release Scripts
- **`on-release.sh`** - Builds release packages and generates package JSONs
- **`upload-release-assets.sh`** - Uploads JSONs to release/Pages and updates version commit
- **`lib-github-release.sh`** - Helper functions for upload and GH API operations
- **`release_append_cn.py`** - Adds CN mirror metadata to package JSONs

---

## Adding a New SoC

When adding support for a new ESP32 SoC variant, follow these steps:

### 1. Update Core Configuration

Edit `.github/scripts/socs_config.sh`:

```bash
# Add to ALL_SOCS (alphabetically)
ALL_SOCS=(
    "esp32"
    "esp32c3"
    "esp32-new"  # ← Add here
    # ...
)

# Add to platform-specific arrays based on capabilities
HW_TEST_TARGETS=(
    # ...
    "esp32-new"  # If physical hardware available
)

WOKWI_TEST_TARGETS=(
    # ...
    "esp32-new"  # If supported by Wokwi
)

QEMU_TEST_TARGETS=(
    # ...
    "esp32-new"  # If supported by QEMU
)
# BUILD_TEST_TARGETS is auto-computed from above
```

### 2. Update IDF Version Targets

```bash
# Add to appropriate IDF version arrays
IDF_V5_5_TARGETS=(
    # ...
    "esp32-new"
)

# Update default if this is the latest
IDF_COMPONENT_TARGETS=("${IDF_V5_5_TARGETS[@]}")
```

### 3. Update Skip List (if applicable)

```bash
# If no pre-built libraries available
SKIP_LIB_BUILD_SOCS=(
    "esp32c2"
    "esp32c61"
    "esp32-new"  # ← Add if needed
)
```

### 4. That's It!

All workflows and scripts will automatically:
- Include the new SoC in builds
- Test on appropriate platforms
- Generate correct matrices
- Use correct QEMU architecture

**No need to modify:**
- Any workflow files
- Any other scripts
- Any test configurations

---

## Official Variants Filter

### Purpose

Prevent unnecessary CI runs when community members contribute custom board variants that aren't officially tested.

### How It Works

The script `.github/scripts/check_official_variants.sh` is called by workflows to determine if builds should run:

```yaml
- name: Check if official variants changed
  id: check-variants
  run: |
    bash .github/scripts/check_official_variants.sh \
      "${{ github.event_name }}" \
      "BUILD_TEST_TARGETS" \
      ${{ steps.changed-files.outputs.all_changed_files }}
```

### Logic

1. **Always run** if not a PR (push, schedule, manual)
2. **Check variants** if PR:
   - Extract variant name from path (e.g., `variants/esp32/...` → `esp32`)
   - Check if variant is in official targets list
   - If official variant found → run
3. **Check non-variant files**:
   - If any non-variant file changed → run
4. **Skip** if only non-official variants changed

### Example Scenarios

| Changes | Result | Reason |
|---------|--------|--------|
| `variants/esp32/pins.h` | ✅ Run | esp32 is official |
| `variants/custom_board/pins.h` | ❌ Skip | custom_board not official |
| `cores/esp32/Arduino.h` | ✅ Run | Non-variant file |
| `variants/esp32/...` + `variants/custom/...` | ✅ Run | Official variant included |
| Push to master | ✅ Run | Not a PR |

### Workflows Using Filter

- `push.yml` - Uses `BUILD_TEST_TARGETS`
- `build_component.yml` - Uses `IDF_COMPONENT_TARGETS`

---

## Testing

### Testing Configuration Changes

After modifying `socs_config.sh`:
1. Source the config file and verify arrays are populated correctly
2. Test helper functions return expected values
3. Check that functions like `get_arch` and `is_qemu_supported` work for all SoCs

### Testing Variant Filter

Test the official variant filter script with different scenarios:
- Official variant changes should return `true`
- Non-official variant changes should return `false`
- Push events should always return `true` regardless of changes

### Running Scripts Locally

Individual scripts can be tested locally:
- Build scripts can run with Arduino CLI installed
- Test scripts require appropriate runtime environment (QEMU/hardware/Wokwi)
- Matrix generation scripts can be run to verify JSON output

Note: GitHub Actions workflows cannot run locally, but the bash scripts they call can be.

---

## Best Practices

### For Contributors

1. **Adding SoCs**: Only edit `socs_config.sh` - all workflows update automatically
2. **Test Requirements**: Use `ci.yml` in test directories to specify requirements
3. **Library Dependencies**: List in `ci.yml` for automatic installation

### For Maintainers

1. **Keep config centralized**: Resist adding SoC lists elsewhere
2. **Document changes**: Update this README when adding workflows
3. **Test locally**: Verify scripts work before pushing
4. **Monitor CI costs**: Use official variant filter to reduce unnecessary runs

---

## Troubleshooting

### Common Issues

**Problem:** Workflow doesn't trigger
- Check `paths` in workflow file
- Verify `socs_config.sh` is in sparse-checkout if used

**Problem:** Build fails for new SoC
- Ensure SoC is in appropriate target lists
- Check if pre-built libs exist (add to `SKIP_LIB_BUILD_SOCS` if not)
- Verify IDF version support

**Problem:** QEMU tests fail
- Check `get_arch()` returns correct architecture
- Verify SoC is in `QEMU_TEST_TARGETS`
- Ensure QEMU version supports the SoC

**Problem:** Variant filter not working
- Verify variant name matches exactly (case-sensitive)
- Check changed files are being passed correctly
- Test script locally with actual file paths

### Getting Help

- **Issues**: Check existing GitHub issues
- **Discussions**: Use GitHub Discussions for questions
- **CI Logs**: Check workflow run logs for detailed errors
- **Scripts**: Most scripts have `--help` or `-h` flags

---

## Maintenance

### Regular Tasks

- **Update IDF versions**: Add new IDF releases to `socs_config.sh`
- **Review external libs**: Update `lib.json` with popular libraries
- **Monitor CI costs**: Review workflow usage and optimize
- **Update dependencies**: Keep actions versions current

### When ESP-IDF Updates

1. Add new version arrays to `socs_config.sh`
2. Update `get_targets_for_idf_version()` function
3. Test builds with new version
4. Update default `IDF_COMPONENT_TARGETS` if needed

### When New SoC Released

1. Follow [Adding a New SoC](#adding-a-new-soc) guide
2. Create board definitions in `variants/`
3. Add to appropriate test matrices
4. Verify all workflows pass

---

## Summary

The ESP32 Arduino Core CI/CD system is designed for:
- **Consistency**: Centralized configuration ensures all parts use the same SoC lists
- **Efficiency**: Official variant filter reduces unnecessary runs
- **Scalability**: Easy to add new SoCs without touching workflows
- **Maintainability**: Single source of truth for all SoC definitions
- **Comprehensiveness**: Tests on multiple platforms (hardware, QEMU, Wokwi)

All SoC management flows through **`.github/scripts/socs_config.sh`** - when in doubt, start there!
