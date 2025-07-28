#!/bin/bash

cd "$(dirname ${BASH_SOURCE[0]})"

echo "Building splatx..."

BUILD_DIR="build-dmg"

mkdir $BUILD_DIR
cd $BUILD_DIR
MACOSX_DEPLOYMENT_TARGET=13.3
cmake \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=13.3 \
    -DDIVERSE_DEPLOY=1 \
    -DCMAKE_BUILD_TYPE="Production" \
    -DCMAKE_OSX_ARCHITECTURES="arm64;" \
    -DGPU_RUNTIME=MPS \
    ../.. || exit 1
make -j8 || exit 1
cd ..
sudo mkdir -p $BUILD_DIR/bin/splatx.app/Contents/Frameworks
# sudo mkdir -p $BUILD_DIR/bin/splatx.app/Contents/Resources/app_icons
sudo mkdir -p $BUILD_DIR/bin/splatx.app/Contents/Resources/fonts
sudo mkdir -p $BUILD_DIR/bin/splatx.app/Contents/Resources/vulkan/icd.d
sudo mkdir -p $BUILD_DIR/bin/splatx.app/Contents/layouts

sudo cp ../application/resource/fonts/simkai.ttf $BUILD_DIR/bin/splatx.app/Contents/Resources/fonts/simkai.ttf
sudo cp ../layouts/dvui.ini $BUILD_DIR/bin/splatx.app/Contents/layouts/dvui.ini
sudo cp /usr/local/lib/libvulkan.1.dylib $BUILD_DIR/bin/splatx.app/Contents/Frameworks
real_dep=$(realpath "/usr/local/lib/libvulkan.1.dylib")
dep_name=$(basename "$real_dep")
sudo cp "$real_dep" "$BUILD_DIR/bin/splatx.app/Contents/Frameworks/$dep_name"

sudo cp /usr/local/lib/libMoltenVK.dylib $BUILD_DIR/bin/splatx.app/Contents/Frameworks
# sudo cp $BUILD_DIR/bin/libgstrain.dylib $BUILD_DIR/bin/splatx.app/Contents/Frameworks
# sudo cp $BUILD_DIR/bin/libtinygsplat.dylib $BUILD_DIR/bin/splatx.app/Contents/Frameworks
# sudo cp $BUILD_DIR/bin/libcolmap_dvs.dylib $BUILD_DIR/bin/splatx.app/Contents/Frameworks
# sudo cp $BUILD_DIR/bin/default.metallib $BUILD_DIR/bin/splatx.app/Contents/Resources/default.metallib
sudo cp $BUILD_DIR/bin/splatx-cli $BUILD_DIR/bin/splatx.app/Contents/MacOS/splatx-cli

sudo python3 modify_dylib.py --appPath $BUILD_DIR/bin/splatx.app
# # 检查参数

APP_PATH= "$BUILD_DIR/bin/splatx.app"
APP_BINARY="$BUILD_DIR/bin/splatx.app/Contents/MacOS/$(basename "$BUILD_DIR/bin/splatx.app" .app)"
FRAMEWORKS_DIR="$BUILD_DIR/bin/splatx.app/Contents/Frameworks"
APP_CLI_BINARY="$BUILD_DIR/bin/splatx.app/Contents/MacOS/splatx-cli"

OPENCV_PATH=$(brew --prefix opencv)  # 自动获取 OpenCV 路径

# 递归复制依赖
copy_dependencies() {
    local bin=$1
    otool -L "$bin" | awk 'NR>1 {print $1}' | grep -E "$OPENCV_PATH|/usr/local|/opt/homebrew|@rpath" | while read dependency; do

        if [[ "$dependency" == @rpath* ]]; then
            if [[ "$bin" == "$APP_BINARY" ]]; then
                dep_dir="$BUILD_DIR/bin"
            else
                dep_dir=$(dirname "$bin")
            fi
            dep_name=$(basename "$dependency")
            real_dep="$dep_dir/$dep_name"
            echo "real_dep: " $real_dep
        else
            real_dep=$(realpath "$dependency")
            dep_name=$(basename "$real_dep")
        fi
        if [ ! -f "$FRAMEWORKS_DIR/$dep_name" ]; then
            sudo cp "$real_dep" "$FRAMEWORKS_DIR/"
            if [[ "$dependency" != @rpath* ]]; then
                sudo cp "$dependency" "$FRAMEWORKS_DIR/"
            fi
            sudo chmod +w "$FRAMEWORKS_DIR/$dep_name"
            copy_dependencies "$real_dep"
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
install_name_tool -add_rpath "@executable_path/../Frameworks" "$APP_CLI_BINARY"

# 签名（替换为你的证书）
find "$FRAMEWORKS_DIR" -name "*.dylib" | while read -r lib; do
codesign --force --verbose=4 --sign "Developer ID Application: yu liu (7358DS45R4)" --timestamp "$lib"
done
find "$FRAMEWORKS_DIR" -name "*.metallib" | while read -r lib; do
sudo codesign --force --verbose=4 --sign "Developer ID Application: yu liu (7358DS45R4)" --timestamp "$lib" 
done

codesign --force --verbose=4 --sign "Developer ID Application: yu liu (7358DS45R4)"  --timestamp "$APP_CLI_BINARY"
codesign --force --verbose=4 --sign "Developer ID Application: yu liu (7358DS45R4)" --options runtime --entitlements splatx.entitlements --timestamp "$APP_BINARY"

# echo "公证....."
# ditto -c -k --keepParent $BUILD_DIR/bin/splatx.app $BUILD_DIR/bin/splatx.zip
# xcrun notarytool submit $BUILD_DIR/bin/splatx.zip --apple-id "liuyu233@icloud.com" --team-id "7358DS45R4" --password "vmpy-wauw-ymop-wfac" --wait
# xcrun stapler staple -v --force $BUILD_DIR/bin/splatx.app

echo "Creating dmg..."
RESULT="../splatx.dmg"
test -f $RESULT
rm $RESULT
./create-dmg/create-dmg --window-size 500 300 --icon-size 96 --volname "splatx Installer" --app-drop-link 360 105 --icon splatx.app 130 105 $RESULT $BUILD_DIR/bin/splatx.app

# echo "Removing temporary build dir..."
# rm -rf $BUILD_DIR
