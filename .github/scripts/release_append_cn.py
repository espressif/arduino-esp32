
#!/usr/bin/env python3

# Arduino IDE provides by default a package file for the ESP32. This causes version conflicts
# when the user tries to use the JSON file with the Chinese mirrors.
#
# The downside is that the Arduino IDE will always warn the user that updates are available as it
# will consider the version from the Chinese mirrors as a pre-release version.
#
# This script is used to append "-cn" to all versions in the package_esp32_index_cn.json file so that
# the user can select the Chinese mirrors without conflicts.
#
# If Arduino ever stops providing the package_esp32_index.json file by default,
# this script can be removed and the tags reverted.

import json

def append_cn_to_versions(obj):
    if isinstance(obj, dict):
        # dfu-util comes from arduino.cc and not from the Chinese mirrors, so we skip it
        if obj.get("name") == "dfu-util":
            return

        for key, value in obj.items():
            if key == "version" and isinstance(value, str):
                if not value.endswith("-cn"):
                    obj[key] = value + "-cn"
            else:
                append_cn_to_versions(value)

    elif isinstance(obj, list):
        for item in obj:
            append_cn_to_versions(item)

def process_json_file(input_path, output_path=None):
    with open(input_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    append_cn_to_versions(data)

    if output_path is None:
        output_path = input_path

    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)

    print(f"Updated JSON written to {output_path}")

if __name__ == "__main__":
    import sys
    if len(sys.argv) < 2:
        print("Usage: python release_append_cn.py input.json [output.json]")
    else:
        input_file = sys.argv[1]
        output_file = sys.argv[2] if len(sys.argv) > 2 else None
        process_json_file(input_file, output_file)
