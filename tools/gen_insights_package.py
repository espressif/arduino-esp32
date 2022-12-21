import os
import sys
import shutil
import json

APP_HEADER_SIZE = 32
VERSION_NAME_OFFSET = APP_HEADER_SIZE + 16
VERSION_NAME_SIZE = 32
PROJECT_NAME_OFFSET = VERSION_NAME_OFFSET + VERSION_NAME_SIZE
PROJECT_NAME_SIZE = 32

# Input path of temporary build directory created by Arduino
BUILD_DIR=sys.argv[1]
# Input project name
PROJ_NAME=sys.argv[2]
# Input path to create output package
TARGET_PATH=sys.argv[3]

def main():
    print("Creating ESP Insights Firmware Package.")
    archive_path = os.path.join(BUILD_DIR, PROJ_NAME)
    out_path = os.path.join(TARGET_PATH, PROJ_NAME)
    
    # Create target archive directories
    os.makedirs(archive_path, exist_ok = True)
    os.makedirs(os.path.join(archive_path, "partition_table"), exist_ok = True)
    os.makedirs(os.path.join(archive_path, "bootloader"), exist_ok = True)
    
    # Copy files from build directory to archive directory
    shutil.copy2(os.path.join(BUILD_DIR, PROJ_NAME + ".bin"), archive_path)
    shutil.copy2(os.path.join(BUILD_DIR, PROJ_NAME + ".elf"), archive_path)
    shutil.copy2(os.path.join(BUILD_DIR, PROJ_NAME + ".map"), archive_path)
    shutil.copy2(os.path.join(BUILD_DIR, "partitions.csv"), archive_path)
    shutil.copy2(os.path.join(BUILD_DIR, PROJ_NAME + ".bootloader.bin"), os.path.join(archive_path, "bootloader"))
    shutil.copy2(os.path.join(BUILD_DIR, PROJ_NAME + ".partitions.bin"), os.path.join(archive_path, "partition_table"))
    
    with open(os.path.join(BUILD_DIR, PROJ_NAME + ".bin"), 'rb') as bin_file:
        bin_file.seek(VERSION_NAME_OFFSET)
        version_name = (bin_file.read(VERSION_NAME_SIZE).decode('utf-8')).split('\x00', 1)[0]
        bin_file.seek(PROJECT_NAME_OFFSET)
        project_name = (bin_file.read(PROJECT_NAME_SIZE).decode('utf-8')).split('\x00', 1)[0]
        project_build_config_obj = {
            "project" : {
                "name" : project_name,
                "version": version_name
            }
        }
        with open(os.path.join(archive_path, "project_build_config.json"), "w") as json_file:
            json_file.write(json.dumps(project_build_config_obj))
    
    shutil.make_archive(out_path, "zip", BUILD_DIR, PROJ_NAME)
    print("Archive created at {}".format(out_path + ".zip"))
    return

if __name__ == '__main__':
    main()