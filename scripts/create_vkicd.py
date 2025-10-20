import os
import argparse


def create_vk_icd(path):
    icd_path = os.path.join(path, "vulkan/icd.d")
    os.makedirs(icd_path, exist_ok=True)
    icd_file = os.path.join(icd_path, "MoltenVK_icd.json")
    with open(icd_file, "w") as f:
        f.write('{\n"file_format_version" : "1.0.0",\n"ICD": {\n"library_path": "../../../Frameworks/libMoltenVK.dylib",\n"api_version" : "1.2.0",\n"is_portability_driver" : true\n}\n}')

def main():
    parser = argparse.ArgumentParser(description="Post-build script")
    parser.add_argument("--resource_path", required=True, help="resource_path")
    parser.add_argument("--verbose", action="store_true", help="Enable verbose output")
    parser.add_argument("--version", help="Project version")

    args = parser.parse_args()
    # resource_path = "/Users/d5/Documents/github/diverse/scripts/build-dmg/bin/diverseshot.app/Contents/Resources"

    if args.verbose:
        print(f"Verbose mode enabled")
    if args.resource_path:
        create_vk_icd(args.resource_path)
    if args.version:
        print(f"Project version: {args.version}")

if __name__ == "__main__":
    main()