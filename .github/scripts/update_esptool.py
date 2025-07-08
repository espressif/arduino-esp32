#!/usr/bin/env python3

# This script is used to re-package the esptool if needed and update the JSON file
# for the Arduino ESP32 platform.
#
# The script has only been tested on macOS.
#
# For regular esptool releases, the generated packages already contain the correct permissions,
# extensions and are uploaded to the GitHub release assets. In this case, the script will only
# update the JSON file with the information from the GitHub release.
#
# The script can be used in two modes:
# 1. Local build: The build artifacts must be already downloaded and extracted in the base_folder.
#    This is useful for esptool versions that are not yet released and that are grabbed from the
#    GitHub build artifacts.
# 2. Release build: The script will get the release information from GitHub and update the JSON file.
#    This is useful for esptool versions that are already released and that are uploaded to the
#    GitHub release assets.
#
#    For local build, the artifacts must be already downloaded and extracted in the base_folder
#    set with the -l option.
#    For example, a base folder "esptool" should contain the following folders extracted directly
#    from the GitHub build artifacts:
#        esptool/esptool-linux-aarch64
#        esptool/esptool-linux-amd64
#        esptool/esptool-linux-armv7
#        esptool/esptool-macos-amd64
#        esptool/esptool-macos-arm64
#        esptool/esptool-windows-amd64

import argparse
import json
import os
import shutil
import stat
import tarfile
import zipfile
import hashlib
import requests
from pathlib import Path

def compute_sha256(filepath):
    sha256 = hashlib.sha256()
    with open(filepath, "rb") as f:
        for block in iter(lambda: f.read(4096), b""):
            sha256.update(block)
    return f"SHA-256:{sha256.hexdigest()}"

def get_file_size(filepath):
    return os.path.getsize(filepath)

def update_json_for_host(tmp_json_path, version, host, url, archiveFileName, checksum, size):
    with open(tmp_json_path) as f:
        data = json.load(f)

    for pkg in data.get("packages", []):
        for tool in pkg.get("tools", []):
            if tool.get("name") == "esptool_py":
                tool["version"] = version

                if url is None:
                    # If the URL is not set, we need to find the old URL and update it
                    for system in tool.get("systems", []):
                        if system.get("host") == host:
                            url = system.get("url").replace(system.get("archiveFileName"), archiveFileName)
                            break
                    else:
                        print(f"No old URL found for host {host}. Using empty URL.")
                        url = ""

                # Preserve existing systems order and update or append the new system
                systems = tool.get("systems", [])
                system_updated = False
                for i, system in enumerate(systems):
                    if system.get("host") == host:
                        systems[i] = {
                            "host": host,
                            "url": url,
                            "archiveFileName": archiveFileName,
                            "checksum": checksum,
                            "size": str(size),
                        }
                        system_updated = True
                        break

                if not system_updated:
                    systems.append({
                        "host": host,
                        "url": url,
                        "archiveFileName": archiveFileName,
                        "checksum": checksum,
                        "size": str(size),
                    })
                tool["systems"] = systems

    with open(tmp_json_path, "w") as f:
        json.dump(data, f, indent=2, sort_keys=False, ensure_ascii=False)
        f.write("\n")

def update_tools_dependencies(tmp_json_path, version):
    with open(tmp_json_path) as f:
        data = json.load(f)

    for pkg in data.get("packages", []):
        for platform in pkg.get("platforms", []):
            for dep in platform.get("toolsDependencies", []):
                if dep.get("name") == "esptool_py":
                    dep["version"] = version

    with open(tmp_json_path, "w") as f:
        json.dump(data, f, indent=2, sort_keys=False, ensure_ascii=False)
        f.write("\n")

def create_archives(version, base_folder):
    archive_files = []

    for dirpath in Path(base_folder).glob("esptool-*"):
        if not dirpath.is_dir():
            continue

        base = dirpath.name[len("esptool-"):]

        if "windows" in dirpath.name:
            zipfile_name = f"esptool-v{version}-{base}.zip"
            print(f"Creating {zipfile_name} from {dirpath} ...")
            with zipfile.ZipFile(zipfile_name, "w", zipfile.ZIP_DEFLATED) as zipf:
                for root, _, files in os.walk(dirpath):
                    for file in files:
                        full_path = os.path.join(root, file)
                        zipf.write(full_path, os.path.relpath(full_path, start=dirpath))
            archive_files.append(zipfile_name)
        else:
            tarfile_name = f"esptool-v{version}-{base}.tar.gz"
            print(f"Creating {tarfile_name} from {dirpath} ...")
            for root, dirs, files in os.walk(dirpath):
                for name in dirs + files:
                    os.chmod(os.path.join(root, name), stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR |
                                                        stat.S_IRGRP | stat.S_IXGRP |
                                                        stat.S_IROTH | stat.S_IXOTH)
            with tarfile.open(tarfile_name, "w:gz") as tar:
                tar.add(dirpath, arcname=dirpath.name)
            archive_files.append(tarfile_name)

    return archive_files

def determine_hosts(archive_name):
    if "linux-amd64" in archive_name:
        return ["x86_64-pc-linux-gnu"]
    elif "linux-armv7" in archive_name:
        return ["arm-linux-gnueabihf"]
    elif "linux-aarch64" in archive_name:
        return ["aarch64-linux-gnu"]
    elif "macos-amd64" in archive_name:
        return ["x86_64-apple-darwin"]
    elif "macos-arm64" in archive_name:
        return ["arm64-apple-darwin"]
    elif "windows-amd64" in archive_name:
        return ["x86_64-mingw32", "i686-mingw32"]
    else:
        return []

def update_json_from_local_build(tmp_json_path, version, base_folder, archive_files):
    for archive in archive_files:
        print(f"Processing archive: {archive}")
        hosts = determine_hosts(archive)
        if not hosts:
            print(f"Skipping unknown archive type: {archive}")
            continue

        archive_path = Path(archive)
        checksum = compute_sha256(archive_path)
        size = get_file_size(archive_path)

        for host in hosts:
            update_json_for_host(tmp_json_path, version, host, None, archive_path.name, checksum, size)

def update_json_from_release(tmp_json_path, version, release_info):
    assets = release_info.get("assets", [])
    for asset in assets:
        if (asset.get("name").endswith(".tar.gz") or asset.get("name").endswith(".zip")) and "esptool" in asset.get("name"):
            asset_fname = asset.get("name")
            print(f"Processing asset: {asset_fname}")
            hosts = determine_hosts(asset_fname)
            if not hosts:
                print(f"Skipping unknown archive type: {asset_fname}")
                continue

            asset_url = asset.get("browser_download_url")
            asset_checksum = asset.get("digest").replace("sha256:", "SHA-256:")
            asset_size = asset.get("size")
            if asset_checksum is None:
                asset_checksum = ""
                print(f"Asset {asset_fname} has no checksum. Please set the checksum in the JSON file.")

            for host in hosts:
                update_json_for_host(tmp_json_path, version, host, asset_url, asset_fname, asset_checksum, asset_size)

def get_release_info(version):
    url = f"https://api.github.com/repos/espressif/esptool/releases/tags/v{version}"
    response = requests.get(url)
    response.raise_for_status()
    return response.json()

def main():
    parser = argparse.ArgumentParser(description="Repack esptool and update JSON metadata.")
    parser.add_argument("version", help="Version of the esptool (e.g. 5.0.dev1)")
    parser.add_argument("-l", "--local", dest="base_folder", help="Enable local build mode and set the base folder with unpacked artifacts")
    args = parser.parse_args()

    script_dir = Path(__file__).resolve().parent
    json_path = (script_dir / "../../package/package_esp32_index.template.json").resolve()
    tmp_json_path = Path(str(json_path) + ".tmp")
    shutil.copy(json_path, tmp_json_path)

    local_build = args.base_folder is not None

    if local_build:
        os.chdir(args.base_folder)
        os.environ['COPYFILE_DISABLE'] = 'true'  # this disables including resource forks in tar files on macOS
        # Clear any existing archive files
        for file in Path(args.base_folder).glob("esptool-*.*"):
            file.unlink()
        archive_files = create_archives(args.version, args.base_folder)
        update_json_from_local_build(tmp_json_path, args.version, args.base_folder, archive_files)
    else:
        release_info = get_release_info(args.version)
        update_json_from_release(tmp_json_path, args.version, release_info)

    print(f"Updating esptool version fields to {args.version}")
    update_tools_dependencies(tmp_json_path, args.version)

    shutil.move(tmp_json_path, json_path)
    print(f"Done. JSON updated at {json_path}")

if __name__ == "__main__":
    main()
