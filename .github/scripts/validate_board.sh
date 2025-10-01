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
    echo ""
    
    # Rule 1: Check build.board format
    echo "Rule 1: Build Board Format Validation"
    echo "====================================="
    validate_build_board_format "$board_name" "$boards_file"
    echo ""

    # Rule 2: Check for required board properties
    echo "Rule 2: Required Properties Validation"
    echo "======================================"
    validate_required_properties "$board_name" "$boards_file"
    echo ""

    # Rule 3: Check for valid partition schemes for available flash sizes
    echo "Rule 3: Partition Scheme Validation"
    echo "==================================="
    validate_partition_schemes "$board_name" "$boards_file"
    echo ""

    # Rule 4: Check for VID and PID consistency
    echo "Rule 4: VID/PID Consistency Validation"
    echo "====================================="
    validate_vid_pid_consistency "$board_name" "$boards_file"
    echo ""

    # Add more validation rules here as needed
    # Rule 5: Future validation rules can be added here
    echo "=========================================="
    print_success "🎉 ALL VALIDATION RULES PASSED for board '$board_name'"
    echo "=========================================="
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
    
    echo "  ✓ build.board is valid: '$build_board_value'"
}

# Rule 2: Check for required board properties
validate_required_properties() {
    local board_name="$1"
    local boards_file="$2"
    
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

    echo "  ✓ Required properties validation completed"
}

# Rule 3: Check for valid partition schemes for available flash sizes
validate_partition_schemes() {
    local board_name="$1"
    local boards_file="$2"
    
    # Get all available flash sizes for this board
    local flash_sizes
    flash_sizes=$(grep "^$board_name.menu.FlashSize\." "$boards_file" | grep "\.build\.flash_size=" | cut -d'=' -f2 | sort -V)
    
    # Check if board has menu.FlashSize entries
    if [ -z "$flash_sizes" ]; then
        # If no menu.FlashSize entries, check if board has build.flash_size entry at least
        local has_flash_size
        has_flash_size=$(grep "^$board_name\." "$boards_file" | grep "\.build\.flash_size=" | head -1)
        
        if [ -z "$has_flash_size" ]; then
            print_error "No flash size options found for board '$board_name' (needs build.flash_size entry at least)"
            exit 1
        else
            # Extract flash size from build.flash_size entry
            local flash_size_value
            flash_size_value=$(echo "$has_flash_size" | cut -d'=' -f2)
            flash_sizes="$flash_size_value"
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
    
    echo "  ✓ Flash configuration found - maximum size: ${max_flash_mb}MB"
    
    # Find all partition schemes for this board
    local partition_schemes
    partition_schemes=$(grep "^$board_name.menu.PartitionScheme\." "$boards_file" | grep -v "\.build\." | grep -v "\.upload\." | sed 's/.*\.PartitionScheme\.\([^=]*\)=.*/\1/' | sort -u)
    
    if [ -n "$partition_schemes" ]; then
        # Validate each partition scheme against the maximum flash size
        while IFS= read -r scheme; do
            validate_partition_scheme_size "$scheme" "$max_flash_mb" "$board_name" "$boards_file"
        done <<< "$partition_schemes"
    fi
    
    
    echo "  ✓ Partition scheme validation completed"
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
        else
            echo "    ✓ Partition scheme '$scheme' is valid for ${max_flash_mb}MB flash (size indicator: ${scheme_size_mb}MB)"
        fi
    elif [[ "$scheme" =~ _([0-9]+)M$ ]]; then
        # Handle cases like "default_8M" (without B)
        local scheme_size_mb="${BASH_REMATCH[1]}"
        
        if [ "$scheme_size_mb" -gt "$max_flash_mb" ]; then
            print_error "Partition scheme '$scheme' (${scheme_size_mb}MB) exceeds available flash size (${max_flash_mb}MB) for board '$board_name'"
            exit 1
        else
            echo "    ✓ Partition scheme '$scheme' is valid for ${max_flash_mb}MB flash (size indicator: ${scheme_size_mb}MB)"
        fi
    elif [[ "$scheme" =~ _([0-9]+)$ ]]; then
        # Handle cases like "esp_sr_16" (just number at end)
        local scheme_size_mb="${BASH_REMATCH[1]}"
        
        if [ "$scheme_size_mb" -gt "$max_flash_mb" ]; then
            print_error "Partition scheme '$scheme' (${scheme_size_mb}MB) exceeds available flash size (${max_flash_mb}MB) for board '$board_name'"
            exit 1
        else
            echo "    ✓ Partition scheme '$scheme' is valid for ${max_flash_mb}MB flash (size indicator: ${scheme_size_mb}MB)"
        fi
    else
        # For schemes without size in name, check description for size indicators
        local description_text
        description_text=$(grep "^$board_name.menu.PartitionScheme\.$scheme=" "$boards_file" | cut -d'=' -f2)
        
        # First check main description for size indicators (before brackets)
        # Look for the largest size indicator in the main description
        local main_description_size_mb=0
        local main_description_size_text=""
        
        # Check for MB and M values in main description (before brackets)
        local main_part=$(echo "$description_text" | sed 's/(.*//')  # Remove bracket content
        
        # Extract M values (not followed by B) and MB values
        local m_values=$(echo "$main_part" | grep -oE '([0-9]+\.?[0-9]*)M' | grep -v 'MB' | sed 's/M$//')
        local mb_values=$(echo "$main_part" | grep -oE '([0-9]+\.?[0-9]*)MB' | sed 's/MB//')
        
        # Combine both M and MB values
        local all_mb_values=$(echo -e "$m_values\n$mb_values" | grep -v '^$')
        
        # Find the largest MB value in main description
        local largest_mb_int=0
        while IFS= read -r value; do
            if [[ "$value" =~ ^([0-9]+)\.([0-9]+)$ ]]; then
                local whole="${BASH_REMATCH[1]}"
                local decimal="${BASH_REMATCH[2]}"
                local value_int=$((whole * 10 + decimal))
            elif [[ "$value" =~ ^([0-9]+)$ ]]; then
                local value_int=$((value * 10))
            else
                continue
            fi
            
            if [ "$value_int" -gt "$largest_mb_int" ]; then
                largest_mb_int=$value_int
                main_description_size_text="${value}M"
            fi
        done <<< "$all_mb_values"
        
        if [ "$largest_mb_int" -gt 0 ]; then
            # Found size in main description
            if [ "$largest_mb_int" -gt $((max_flash_mb * 10)) ]; then
                print_error "Partition scheme '$scheme' (${main_description_size_text} from description) exceeds available flash size (${max_flash_mb}MB) for board '$board_name'"
                exit 1
            else
                echo "    ✓ Partition scheme '$scheme' is valid for ${max_flash_mb}MB flash (size from description: ${main_description_size_text})"
            fi
        else
            # No size in main description, check bracket content
            local bracket_content
            bracket_content=$(echo "$description_text" | grep -oE '\([^)]+\)' | head -1)
            
            if [ -n "$bracket_content" ]; then
                # Calculate total size from all components in brackets
                local total_size_mb=0
                
                # Extract and sum MB values
                local mb_sum=0
                while IFS= read -r value; do
                    if [[ "$value" =~ ^([0-9]+)\.([0-9]+)$ ]]; then
                        local whole="${BASH_REMATCH[1]}"
                        local decimal="${BASH_REMATCH[2]}"
                        # Convert decimal to integer (e.g., 1.3 -> 13, 6.93 -> 69)
                        # Handle multi-digit decimals: 6.93 -> 6*10 + 9 = 69 (round down)
                        local decimal_int=$((decimal / 10))
                        mb_sum=$((mb_sum + whole * 10 + decimal_int))
                    elif [[ "$value" =~ ^([0-9]+)$ ]]; then
                        mb_sum=$((mb_sum + value * 10))
                    fi
                done < <(echo "$bracket_content" | grep -oE '([0-9]+\.?[0-9]*)MB' | sed 's/MB//')
                
                # Extract and sum KB values (convert to MB tenths)
                local kb_sum=0
                while IFS= read -r value; do
                    if [[ "$value" =~ ^([0-9]+)\.([0-9]+)$ ]]; then
                        local whole="${BASH_REMATCH[1]}"
                        local decimal="${BASH_REMATCH[2]}"
                        # Convert KB to MB tenths: (whole.decimal * 10) / 1024, rounded
                        local kb_tenths=$((whole * 10 + decimal))
                        kb_sum=$((kb_sum + (kb_tenths * 10 + 512) / 1024))
                    elif [[ "$value" =~ ^([0-9]+)$ ]]; then
                        # Convert KB to MB tenths: value * 10 / 1024, rounded
                        kb_sum=$((kb_sum + (value * 10 + 512) / 1024))
                    fi
                done < <(echo "$bracket_content" | grep -oE '([0-9]+\.?[0-9]*)KB' | sed 's/KB//')
                
                # Sum all values and convert back to MB (divide by 10, rounded)
                total_size_mb=$(( (mb_sum + kb_sum + 5) / 10 ))
                
                if [ "$total_size_mb" -gt 0 ]; then
                    # Found size in description
                    if [ "$total_size_mb" -gt "$max_flash_mb" ]; then
                        print_error "Partition scheme '$scheme' (${total_size_mb}MB from description) exceeds available flash size (${max_flash_mb}MB) for board '$board_name'"
                        exit 1
                    else
                        echo "    ✓ Partition scheme '$scheme' is valid for ${max_flash_mb}MB flash (size from description: ${total_size_mb}MB)"
                    fi
                else
                    # No size indicator found in brackets, check upload maximum size
                    validate_scheme_upload_size "$scheme" "$board_name" "$boards_file" "$max_flash_mb"
                fi
            else
                # No brackets found, check upload maximum size
                validate_scheme_upload_size "$scheme" "$board_name" "$boards_file" "$max_flash_mb"
            fi
        fi
    fi
}

# Helper function to validate upload maximum size for a specific partition scheme
validate_scheme_upload_size() {
    local scheme="$1"
    local board_name="$2"
    local boards_file="$3"
    local max_flash_mb="$4"
    
    # Get upload maximum size for this specific scheme
    local upload_size
    upload_size=$(grep "^$board_name.menu.PartitionScheme\.$scheme\." "$boards_file" | grep "\.upload\.maximum_size=" | head -1 | cut -d'=' -f2)
    
    if [ -z "$upload_size" ]; then
        echo "    ✓ Partition scheme '$scheme' is valid for ${max_flash_mb}MB flash (no upload size limit)"
        return 0
    fi
    
    # Convert flash size to bytes for comparison
    local max_flash_bytes=$((max_flash_mb * 1024 * 1024))
    
    # Check upload size against maximum flash size
    if [ "$upload_size" -gt "$max_flash_bytes" ]; then
        local upload_mb=$(( (upload_size + 524288) / 1048576 ))
        print_error "Partition scheme '$scheme' upload size (${upload_mb}MB, ${upload_size} bytes) exceeds available flash size (${max_flash_mb}MB) for board '$board_name'"
        exit 1
    fi
    
    local upload_mb=$(( (upload_size + 524288) / 1048576 ))
    echo "    ✓ Partition scheme '$scheme' is valid for ${max_flash_mb}MB flash (upload size: ${upload_mb}MB)"
}

# Rule 4: Check for VID and PID consistency
validate_vid_pid_consistency() {
    local board_name="$1"
    local boards_file="$2"
    
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
    
    echo "  ✓ VID and PID consistency check passed"
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
