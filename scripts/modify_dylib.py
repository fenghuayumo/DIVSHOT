import os
import subprocess

def get_dependencies(dylib_path):
    """
    获取指定 dylib 文件的所有依赖项
    """
    cmd = f"otool -L {dylib_path}"
    output = subprocess.check_output(cmd, shell=True).decode()
    dependencies = []
    for line in output.split("\n")[1:]:
        dep = line.strip().split(" (")[0]
        if os.path.basename(dep) == os.path.basename(dylib_path):
            continue
        if dep and not dep.startswith("/usr/") and \
            not dep.startswith("/System/") and not dep.startswith("@rpath") and \
            not dep.startswith("@executable_path") and not dep.startswith("@loader_path"):
            dependencies.append(dep)
    return dependencies

def update_dependency(old_path, new_path, target_file):
    """
    修改指定文件中对动态库的引用
    """
    cmd = f"sudo install_name_tool -change {old_path} {new_path} {target_file}"
    os.system(cmd)
    # print(cmd)

def update_id(new_name, dypath):
    """
    修改动态库的名称
    """
    cmd = f"sudo install_name_tool -id {new_name} {dypath}"
    # os.system(cmd)

def copy_dylib_dependencies(dylib_path, frameworks_dir,depth=0):
    """
    遍历 dylib 文件的所有依赖项,并修改它们的名称和安装路径
    """
    dependencies = get_dependencies(dylib_path)
    for dep in dependencies:
        new_path = os.path.join("@rpath", os.path.basename(dep))
        new_full_path = os.path.join(frameworks_dir, os.path.basename(dep))
        cmd = f"sudo cp {dep} {new_full_path}"
        os.system(cmd)
        if depth < 2:
            copy_dylib_dependencies(new_full_path, frameworks_dir,depth+1)

def update_app_dependencies(dylib_path, frameworks_dir):
    dependencies = get_dependencies(dylib_path)
    for dep in dependencies:
        new_path = os.path.join("@rpath", os.path.basename(dep))
        new_full_path = os.path.join(frameworks_dir, os.path.basename(dep))
        update_dependency(dep, new_path, dylib_path)
        update_id(new_path, new_full_path)

def update_dylib_dependencies(frameworks_dir):
    # get frameworks_dir all dylib
    dylibs = []
    for root, dirs, files in os.walk(frameworks_dir):
        for file in files:
            if file.endswith(".dylib"):
                dylibs.append(os.path.join(root, file))
    for dylib in dylibs:
        dependencies = get_dependencies(dylib)
        for dep in dependencies:
            new_path = os.path.join("@rpath", os.path.basename(dep))
            new_full_path = os.path.join(frameworks_dir, os.path.basename(dep))
            update_dependency(dep, new_path, dylib)
            update_id(new_full_path, new_full_path)


def update_all_dependencies(app_path, frameworks_dir):
    copy_dylib_dependencies(app_path, frameworks_dir)
    update_app_dependencies(app_path, frameworks_dir)
    update_dylib_dependencies(frameworks_dir)

def create_vk_icd(path):
    # create vulkan/icd.d/MoltenVK_icd.json and write content 
    # {
    # "file_format_version" : "1.0.0",
    # "ICD": {
    #     "library_path": "../../../Frameworks/libMoltenVK.dylib",
    #     "api_version" : "1.2.0",
    #     "is_portability_driver" : true
    # }
    # }
    icd_path = os.path.join(path, "vulkan/icd.d")
    os.makedirs(icd_path, exist_ok=True)
    icd_file = os.path.join(icd_path, "MoltenVK_icd.json")
    with open(icd_file, "w") as f:
        f.write('{\n"file_format_version" : "1.0.0",\n"ICD": {\n"library_path": "../../../Frameworks/libMoltenVK.dylib",\n"api_version" : "1.2.0",\n"is_portability_driver" : true\n}\n}')
    
# app_path = "/Users/d5/Documents/github/diverse/scripts/build-dmg/diverseshot.app/Contents/MacOS/diverseshot"
# frameworks_dir = "/Users/d5/Documents/github/diverse/scripts/build-dmg/diverseshot.app/Contents/Frameworks"
resource_path = "/Users/d5/Documents/github/diverse/scripts/build-dmg/bin/diverseshot.app/Contents/Resources"
# update_all_dependencies(app_path, frameworks_dir)

create_vk_icd(resource_path)
