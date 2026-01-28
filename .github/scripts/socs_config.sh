#!/bin/bash

#
# Centralized SoC Configuration
# This file contains all supported SoC definitions for the ESP32 Arduino core.
# Update this file when adding or removing SoC support.
# Keep the SoCs sorted alphabetically.
#

# ==============================================================================
# Core SoC Definitions
# ==============================================================================

# All supported SoCs
ALL_SOCS=(
    "esp32"
    "esp32c2"
    "esp32c3"
    "esp32c5"
    "esp32c6"
    "esp32c61"
    "esp32h2"
    "esp32p4"
    "esp32s2"
    "esp32s3"
)

# All supported SoC variants by the ESP32 Arduino core
CORE_VARIANTS=(
    "esp32"
    "esp32c3"
    "esp32c5"
    "esp32c6"
    "esp32h2"
    "esp32p4"
    "esp32p4_es"
    "esp32s2"
    "esp32s3"
)

# SoCs to skip in library builds (no pre-built libs available)
SKIP_LIB_BUILD_SOCS=(
    "esp32c2"
    "esp32c61"
)

# SoCs that are directly supported by the ESP32 Arduino core
# These are ALL_SOCS without SKIP_LIB_BUILD_SOCS
CORE_SOCS=()

# ==============================================================================
# Test Platform Targets
# ==============================================================================

# Hardware test targets (physical devices available in GitLab)
HW_TEST_TARGETS=(
    "esp32"
    "esp32c3"
    "esp32c5"
    "esp32c6"
    "esp32h2"
    "esp32p4"
    "esp32s2"
    "esp32s3"
)

# Wokwi simulator test targets (targets supported by Wokwi)
WOKWI_TEST_TARGETS=(
    "esp32"
    "esp32c3"
    "esp32c6"
    "esp32h2"
    "esp32p4"
    "esp32s2"
    "esp32s3"
)

# QEMU emulator test targets (targets supported by QEMU)
QEMU_TEST_TARGETS=(
    "esp32"
    "esp32c3"
)

# Build test targets (dynamically computed as union of all test targets)
# This combines HW_TEST_TARGETS, WOKWI_TEST_TARGETS, and QEMU_TEST_TARGETS
# The union is computed below after the individual arrays are defined
BUILD_TEST_TARGETS=()

# ==============================================================================
# IDF Component Targets (version-specific)
# ==============================================================================

# IDF v5.3 supported targets
IDF_V5_3_TARGETS=(
    "esp32"
    "esp32c2"
    "esp32c3"
    "esp32c6"
    "esp32h2"
    "esp32p4"
    "esp32s2"
    "esp32s3"
)

# IDF v5.4 supported targets
IDF_V5_4_TARGETS=(
    "esp32"
    "esp32c2"
    "esp32c3"
    "esp32c6"
    "esp32h2"
    "esp32p4"
    "esp32s2"
    "esp32s3"
)

# IDF v5.5 supported targets
IDF_V5_5_TARGETS=(
    "esp32"
    "esp32c2"
    "esp32c3"
    "esp32c5"
    "esp32c6"
    "esp32c61"
    "esp32h2"
    "esp32p4"
    "esp32s2"
    "esp32s3"
)

# Default IDF component targets (latest version)
IDF_COMPONENT_TARGETS=("${IDF_V5_5_TARGETS[@]}")

# ==============================================================================
# Helper Functions for Array to String Conversion
# ==============================================================================

# Convert array to comma-separated string
# Usage: array_to_csv "${ARRAY[@]}"
array_to_csv() {
    local IFS=','
    echo "$*"
}

# Convert array to JSON array string
# Usage: array_to_json "${ARRAY[@]}"
array_to_json() {
    local first=true
    local result="["
    for item in "$@"; do
        if [ "$first" = true ]; then
            first=false
        else
            result+=","
        fi
        result+="\"$item\""
    done
    result+="]"
    echo "$result"
}

# Convert array to quoted comma-separated string for bash
# Usage: array_to_quoted_csv "${ARRAY[@]}"
array_to_quoted_csv() {
    local first=true
    local result=""
    for item in "$@"; do
        if [ "$first" = true ]; then
            first=false
        else
            result+=","
        fi
        result+="\"$item\""
    done
    echo "$result"
}

# ==============================================================================
# QEMU Specific Configuration
# ==============================================================================

# Check if SoC is supported by QEMU
# Usage: is_qemu_supported "esp32c3"
# Returns: 0 if supported, 1 otherwise
is_qemu_supported() {
    local soc="$1"
    for target in "${QEMU_TEST_TARGETS[@]}"; do
        if [ "$target" = "$soc" ]; then
            return 0
        fi
    done
    return 1
}

# ==============================================================================
# IDF Specific Configuration
# ==============================================================================

# Get IDF targets for a specific version
# Usage: get_targets_for_idf_version "release-v5.5"
get_targets_for_idf_version() {
    local version="$1"
    case "$version" in
        release-v5.3)
            array_to_csv "${IDF_V5_3_TARGETS[@]}"
            ;;
        release-v5.4)
            array_to_csv "${IDF_V5_4_TARGETS[@]}"
            ;;
        release-v5.5)
            array_to_csv "${IDF_V5_5_TARGETS[@]}"
            ;;
        *)
            echo ""
            ;;
    esac
}

# ==============================================================================
# SoC Properties
# ==============================================================================

# Get architecture for a given SoC
# Usage: get_arch "esp32s3"
# Returns: "xtensa" or "riscv32"
get_arch() {
    local soc="$1"
    case "$soc" in
        esp32|esp32s2|esp32s3)
            echo "xtensa"
            ;;
        *)
            echo "riscv32"
            ;;
    esac
}

# Check if a SoC should be skipped in library builds
# Usage: should_skip_lib_build "esp32c2"
# Returns: 0 if should skip, 1 otherwise
should_skip_lib_build() {
    local soc="$1"
    for skip_soc in "${SKIP_LIB_BUILD_SOCS[@]}"; do
        if [ "$skip_soc" = "$soc" ]; then
            return 0
        fi
    done
    return 1
}

# ==============================================================================
# Computed Arrays
# ==============================================================================

# Compute CORE_SOCS as ALL_SOCS without SKIP_LIB_BUILD_SOCS
_compute_core_socs() {
    for soc in "${ALL_SOCS[@]}"; do
        local skip=false
        for skip_soc in "${SKIP_LIB_BUILD_SOCS[@]}"; do
            if [ "$soc" = "$skip_soc" ]; then
                skip=true
                break
            fi
        done
        if [ "$skip" = false ]; then
            echo "$soc"
        fi
    done
}

while IFS= read -r soc; do
    CORE_SOCS+=("$soc")
done < <(_compute_core_socs)

# Compute BUILD_TEST_TARGETS as the union of all test platform targets that have pre-built libraries
_compute_build_test_targets() {
    # Combine all test targets that have pre-built libraries, remove duplicates, and sort
    printf '%s\n' "${HW_TEST_TARGETS[@]}" "${WOKWI_TEST_TARGETS[@]}" "${QEMU_TEST_TARGETS[@]}" | sort -u
}

while IFS= read -r target; do
    BUILD_TEST_TARGETS+=("$target")
done < <(_compute_build_test_targets)

# ==============================================================================
# Main Exports (for scripts that source this file)
# ==============================================================================

ALL_SOCS_CSV=$(array_to_csv "${ALL_SOCS[@]}")
CORE_SOCS_CSV=$(array_to_csv "${CORE_SOCS[@]}")
CORE_VARIANTS_CSV=$(array_to_csv "${CORE_VARIANTS[@]}")
HW_TEST_TARGETS_CSV=$(array_to_csv "${HW_TEST_TARGETS[@]}")
WOKWI_TEST_TARGETS_CSV=$(array_to_csv "${WOKWI_TEST_TARGETS[@]}")
QEMU_TEST_TARGETS_CSV=$(array_to_csv "${QEMU_TEST_TARGETS[@]}")
BUILD_TEST_TARGETS_CSV=$(array_to_csv "${BUILD_TEST_TARGETS[@]}")
IDF_COMPONENT_TARGETS_CSV=$(array_to_csv "${IDF_COMPONENT_TARGETS[@]}")

# Export commonly used variables for backward compatibility
export ALL_SOCS_CSV
export CORE_SOCS_CSV
export CORE_VARIANTS_CSV
export HW_TEST_TARGETS_CSV
export WOKWI_TEST_TARGETS_CSV
export QEMU_TEST_TARGETS_CSV
export BUILD_TEST_TARGETS_CSV
export IDF_COMPONENT_TARGETS_CSV

