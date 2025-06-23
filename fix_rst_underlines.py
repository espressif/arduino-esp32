#!/usr/bin/env python3
import os
import glob
import re

def fix_rst_underlines(file_path):
    print(f"Processing: {file_path}")
    
    with open(file_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    
    new_lines = []
    i = 0
    while i < len(lines):
        line = lines[i]
        
        # Check if this line is an underline (only contains -, *, or ^)
        if i > 0 and re.match(r'^[-*^]+$', line.strip()):
            prev_line = lines[i-1].rstrip('\n')
            # Skip if previous line is empty or also an underline
            if prev_line and not re.match(r'^[-*^#]+$', prev_line):
                underline_char = line.strip()[0]
                underline = underline_char * len(prev_line)
                new_lines.append(underline + '\n')
                print(f"  Fixed underline: '{prev_line}' -> {len(prev_line)} chars")
            else:
                new_lines.append(line)
        else:
            new_lines.append(line)
        i += 1
    
    with open(file_path, 'w', encoding='utf-8') as f:
        f.writelines(new_lines)

if __name__ == "__main__":
    rst_files = glob.glob("docs/en/zigbee/*.rst")
    print(f"Found {len(rst_files)} RST files to process")
    
    for rst_file in rst_files:
        fix_rst_underlines(rst_file)
    
    print("All subtitle underlines have been fixed.") 