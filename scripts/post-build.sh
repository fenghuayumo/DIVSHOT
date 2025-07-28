#!/bin/bash

# echo "名字: $1"
APP_PATH="$1"
APP_BINARY="$APP_PATH/Contents/MacOS/$(basename "$APP_PATH" .app)"
FRAMEWORKS_DIR="$APP_PATH/Contents/Frameworks"
# APP_CLI_BINARY="$APP_BINARY/../../Frameworks/diverseshot-cli"

real_dep=$(realpath "/usr/local/lib/libvulkan.1.dylib")
dep_name=$(basename "$real_dep")
sudo cp "$real_dep" "$FRAMEWORKS_DIR/$dep_name"

OPENCV_PATH=$(brew --prefix opencv)  # 自动获取 OpenCV 路径

# 递归复制依赖
copy_dependencies() {
    local bin=$1
    otool -L "$bin" | awk 'NR>1 {print $1}' | grep -E "$OPENCV_PATH|/usr/local|/opt/homebrew" | while read dependency; do
        real_dep=$(realpath "$dependency")
        dep_name=$(basename "$real_dep")
        if [ ! -f "$FRAMEWORKS_DIR/$dep_name" ]; then
            sudo cp "$real_dep" "$FRAMEWORKS_DIR/"
            sudo cp "$dependency" "$FRAMEWORKS_DIR/"
            sudo chmod +w "$FRAMEWORKS_DIR/$dep_name"
            copy_dependencies "$FRAMEWORKS_DIR/$dep_name"
        fi
    done
}

copy_dependencies "$APP_BINARY"

fix_rpath() {
    local lib=$1
    # echo "修正路径: $lib"
    # codesign --remove-signature "$lib" >/dev/null 2>&1 || true
    chmod +w "$lib"
    sudo install_name_tool -id "@rpath/$(basename "$lib")" "$lib"
    otool -L "$lib" | awk 'NR>1 {print $1}' | grep -E "$OPENCV_PATH|/usr/local|/opt/homebrew" | while read dependency; do
        echo "修正依赖: $dependency"
        sudo install_name_tool -change "$dependency" "@rpath/$(basename "$dependency")" "$lib"
    done
}

find "$FRAMEWORKS_DIR" -name "*.dylib" | while read -r lib; do
    fix_rpath "$lib"
done

# 主程序 RPATH
otool -L "$APP_BINARY" | awk 'NR>1 {print $1}' | grep -E "$OPENCV_PATH|/usr/local|/opt/homebrew" | while read dependency; do
    echo "修正依赖: $dependency"
    sudo install_name_tool -change "$dependency" "@rpath/$(basename "$dependency")" "$APP_BINARY"
done
install_name_tool -add_rpath "@executable_path/../Frameworks" "$APP_BINARY"
# install_name_tool -add_rpath "@executable_path/../Frameworks" "$APP_CLI_BINARY"