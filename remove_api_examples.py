#!/usr/bin/env python3
import re
import glob

def remove_api_examples(file_path):
    print(f"Processing: {file_path}")
    
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Pattern to match individual API examples
    # Matches from "**Example:**" to the next method or section
    pattern = r'\*\*Example:\*\*\s*\n\s*\n\s*\.\. code-block:: arduino\s*\n\s*\n\s*[^.]*?\.\s*\n\s*\n'
    
    # Remove all individual API examples
    modified_content = re.sub(pattern, '', content, flags=re.MULTILINE | re.DOTALL)
    
    # Also remove any remaining "**Example:**" lines that might be left
    modified_content = re.sub(r'\*\*Example:\*\*\s*\n', '', modified_content)
    
    # Clean up any double newlines that might be left
    modified_content = re.sub(r'\n\s*\n\s*\n', '\n\n', modified_content)
    
    with open(file_path, 'w', encoding='utf-8') as f:
        f.write(modified_content)
    
    print(f"  Removed individual API examples from {file_path}")

if __name__ == "__main__":
    rst_files = glob.glob("*.rst")
    print(f"Found {len(rst_files)} RST files to process")
    
    for rst_file in rst_files:
        remove_api_examples(rst_file)
    
    print("All individual API examples have been removed.") 