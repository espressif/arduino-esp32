#!/bin/bash

# Board validation script for ESP32 Arduino Core
# This script validates board definitions in boards.txt

set -e

# Function to print output
print_error() {
    echo "Error: $1"
}

print_success() {
    echo "✓ $1"
}

# Function to validate a single board
validate_board() {
    local board_name="$1"
    local boards_file="boards.txt"
    
    echo "Validating board: $board_name"
    
    # Rule 1: Check build.board format
    validate_build_board_format "$board_name" "$boards_file"

    # Rule 2: Check for required board properties
    validate_required_properties "$board_name" "$boards_file"

    # Rule 3: Check for valid partition schemes for available flash sizes
    validate_partition_schemes "$board_name" "$boards_file"

    # Rule 4: Check for VID and PID consistency
    validate_vid_pid_consistency "$board_name" "$boards_file"

    # Add more validation rules here as needed
    # Rule 5: Future validation rules can be added here
    print_success "Board '$board_name' validation passed"
}

# Rule 1: Check build.board format
validate_build_board_format() {
    local board_name="$1"
    local boards_file="$2"
    
    # Get the build.board value
    local build_board_value
    build_board_value=$(grep "^$board_name.build.board=" "$boards_file" | cut -d'=' -f2)
    
    if [ -z "$build_board_value" ]; then
        print_error "build.board property not found for '$board_name'"
        exit 1
    fi
    
    # Check for invalid characters (anything that's not uppercase letter, number, or underscore)
    if echo "$build_board_value" | grep -q '[^A-Z0-9_]'; then
        local invalid_chars
        invalid_chars=$(echo "$build_board_value" | grep -o '[^A-Z0-9_]' | sort -u | tr -d '\n')
        print_error "$board_name.build.board contains invalid characters: '$invalid_chars' (only A-Z, 0-9, and _ are allowed)"
        exit 1
    fi
    
    # Check if it's all uppercase
    if echo "$build_board_value" | grep -q '[a-z]'; then
        print_error "build.board must be uppercase: '$build_board_value' (found lowercase letters)"
        exit 1
    fi
    
    print_success "build.board is valid: '$build_board_value'"
}

# Rule 2: Check for required board properties
validate_required_properties() {
    local board_name="$1"
    local boards_file="$2"
    
    echo "Checking required board properties..."
    local required_props=("upload.flags" "upload.extra_flags")
    local missing_props=()
    
    for prop in "${required_props[@]}"; do
        if ! grep -q "^$board_name.$prop=" "$boards_file"; then
            missing_props+=("$prop")
        fi
    done
    
    if [ ${#missing_props[@]} -gt 0 ]; then
        print_error "Missing required properties for board '$board_name':"
        printf '  - %s\n' "${missing_props[@]}"
        exit 1
    fi
}

# Rule 3: Check for valid partition schemes for available flash sizes
validate_partition_schemes() {
    local board_name="$1"
    local boards_file="$2"
    
    echo "Checking partition schemes for available flash sizes..."
    
    # Get all available flash sizes for this board
    local flash_sizes
    flash_sizes=$(grep "^$board_name.menu.FlashSize\." "$boards_file" | grep "\.build\.flash_size=" | cut -d'=' -f2 | sort -V)
    
    # Check if board has menu.FlashSize entries
    if [ -z "$flash_sizes" ]; then
        # If no menu.FlashSize entries, check if board has any build.flash_size entry
        local has_flash_size
        has_flash_size=$(grep "^$board_name\." "$boards_file" | grep "\.build\.flash_size=" | head -1)
        
        if [ -z "$has_flash_size" ]; then
            print_error "No flash size options found for board '$board_name' (needs either menu.FlashSize entries or build.flash_size entry)"
            exit 1
        else
            print_success "Board '$board_name' has build.flash_size entry (no menu.FlashSize required)"
            return 0
        fi
    fi
    
    # Convert flash sizes to MB for comparison
    local flash_sizes_mb=()
    while IFS= read -r size; do
        if [[ "$size" =~ ^([0-9]+)MB$ ]]; then
            flash_sizes_mb+=("${BASH_REMATCH[1]}")
        fi
    done <<< "$flash_sizes"
    
    # Find the maximum flash size available
    local max_flash_mb=0
    for size_mb in "${flash_sizes_mb[@]}"; do
        if [ "$size_mb" -gt "$max_flash_mb" ]; then
            max_flash_mb="$size_mb"
        fi
    done
    
    echo "  Maximum flash size available: ${max_flash_mb}MB"
    
    # Find all partition schemes for this board
    local partition_schemes
    partition_schemes=$(grep "^$board_name.menu.PartitionScheme\." "$boards_file" | grep "\.build\.partitions=" | cut -d'=' -f2 | sort -u)
    
    if [ -n "$partition_schemes" ]; then
        # Validate each partition scheme against the maximum flash size
        while IFS= read -r scheme; do
            validate_partition_scheme_size "$scheme" "$max_flash_mb" "$board_name" "$boards_file"
        done <<< "$partition_schemes"
    fi
    
    
    print_success "Partition scheme validation completed"
}

# Helper function to validate individual partition scheme
validate_partition_scheme_size() {
    local scheme="$1"
    local max_flash_mb="$2"
    local board_name="$3"
    local boards_file="$4"
    
    # Extract size from partition scheme name (e.g., "default_8MB" -> 8)
    if [[ "$scheme" =~ _([0-9]+)MB$ ]]; then
        local scheme_size_mb="${BASH_REMATCH[1]}"
        
        if [ "$scheme_size_mb" -gt "$max_flash_mb" ]; then
            print_error "Partition scheme '$scheme' (${scheme_size_mb}MB) exceeds available flash size (${max_flash_mb}MB) for board '$board_name'"
            exit 1
        fi
    elif [[ "$scheme" =~ _([0-9]+)M$ ]]; then
        # Handle cases like "default_8M" (without B)
        local scheme_size_mb="${BASH_REMATCH[1]}"
        
        if [ "$scheme_size_mb" -gt "$max_flash_mb" ]; then
            print_error "Partition scheme '$scheme' (${scheme_size_mb}MB) exceeds available flash size (${max_flash_mb}MB) for board '$board_name'"
            exit 1
        fi
    else
        # For schemes without size in name (like "default", "minimal"), check upload maximum size
        validate_scheme_upload_size "$scheme" "$board_name" "$boards_file" "$max_flash_mb"
    fi
}

# Rule 4: Check for VID and PID consistency
validate_vid_pid_consistency() {
    local board_name="$1"
    local boards_file="$2"
    
    echo "Checking VID and PID consistency..."
    
    # Get all VID and PID entries for this board
    local vid_entries
    local pid_entries
    
    vid_entries=$(grep "^$board_name\.vid\." "$boards_file" | sort)
    pid_entries=$(grep "^$board_name\.pid\." "$boards_file" | sort)
    
    # Check for duplicate VID entries with same index but different values
    local vid_duplicates
    vid_duplicates=$(echo "$vid_entries" | cut -d'=' -f1 | sort | uniq -d)
    
    if [ -n "$vid_duplicates" ]; then
        print_error "Found duplicate VID entries with different values for board '$board_name':"
        echo "$vid_duplicates"
        exit 1
    fi
    
    # Check for duplicate PID entries with same index but different values
    local pid_duplicates
    pid_duplicates=$(echo "$pid_entries" | cut -d'=' -f1 | sort | uniq -d)
    
    if [ -n "$pid_duplicates" ]; then
        print_error "Found duplicate PID entries with different values for board '$board_name':"
        echo "$pid_duplicates"
        exit 1
    fi
    
    # Check for missing corresponding PID for each VID (and vice versa)
    local vid_indices
    local pid_indices
    
    vid_indices=$(echo "$vid_entries" | cut -d'=' -f1 | sed "s/^$board_name\.vid\.//" | sort -n)
    pid_indices=$(echo "$pid_entries" | cut -d'=' -f1 | sed "s/^$board_name\.pid\.//" | sort -n)
    
    # Check if VID and PID indices match
    if [ "$vid_indices" != "$pid_indices" ]; then
        print_error "VID and PID indices don't match for board '$board_name'"
        echo "VID indices: $vid_indices"
        echo "PID indices: $pid_indices"
        exit 1
    fi
    
    print_success "VID and PID consistency check passed"
}

# Helper function to validate upload maximum size for a specific partition scheme
validate_scheme_upload_size() {
    local scheme="$1"
    local board_name="$2"
    local boards_file="$3"
    local max_flash_mb="$4"
    
    # Get upload maximum size for this specific scheme
    local upload_size
    upload_size=$(grep "^$board_name.menu.PartitionScheme\..*\.build\.partitions=$scheme" "$boards_file" -A1 | grep "\.upload\.maximum_size=" | head -1 | cut -d'=' -f2)
    
    if [ -z "$upload_size" ]; then
        print_success "Partition scheme '$scheme' is valid for ${max_flash_mb}MB flash (no upload size limit)"
        return 0
    fi
    
    # Convert flash size to bytes for comparison
    local max_flash_bytes=$((max_flash_mb * 1024 * 1024))
    
    # Check upload size against maximum flash size
    if [ "$upload_size" -gt "$max_flash_bytes" ]; then
        local upload_mb=$((upload_size / 1024 / 1024))
        print_error "Partition scheme '$scheme' upload size (${upload_mb}MB, ${upload_size} bytes) exceeds available flash size (${max_flash_mb}MB) for board '$board_name'"
        exit 1
    fi
    
    local upload_mb=$((upload_size / 1024 / 1024))
    print_success "Partition scheme '$scheme' is valid for ${max_flash_mb}MB flash (upload size: ${upload_mb}MB)"
}

# Future validation rules can be added here
# Example:
# validate_custom_rule() {
#     local board_name="$1"
#     local boards_file="$2"
#     # Add custom validation logic here
# }

# Main execution
main() {
    if [ $# -ne 1 ]; then
        echo "Usage: $0 <board_name>"
        echo "Example: $0 esp32s3"
        exit 1
    fi
    
    local board_name="$1"
    local boards_file="boards.txt"
    
    if [ ! -f "$boards_file" ]; then
        print_error "Boards file '$boards_file' not found"
        exit 1
    fi
    
    validate_board "$board_name"
}

# Run main function with all arguments
main "$@"
