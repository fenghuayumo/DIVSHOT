PATH="/usr/local/bin:$PATH"
xcodebuild -project runtime.xcodeproj -configuration Release -parallelizeTargets -jobs 4 -sdk macosx -arch x86_64
echo "Build Finished!"