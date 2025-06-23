import os
import glob

def fix_rst_underlines(file_path):
    with open(file_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    new_lines = []
    i = 0
    while i < len(lines):
        line = lines[i]
        # Check for underline (---, ***, ^^^) and not at the top of the file
        if i > 0 and (set(line.strip()) <= set('-') or set(line.strip()) <= set('*') or set(line.strip()) <= set('^')):
            prev_line = lines[i-1].rstrip('\n')
            underline_char = line.strip()[0]
            underline = underline_char * len(prev_line)
            new_lines.append(underline + '\n')
            i += 1
        else:
            new_lines.append(line)
            i += 1

    with open(file_path, 'w', encoding='utf-8') as f:
        f.writelines(new_lines)

if __name__ == "__main__":
    rst_files = glob.glob("docs/en/zigbee/*.rst")
    for rst_file in rst_files:
        fix_rst_underlines(rst_file)
    print("All subtitle underlines have been fixed.")