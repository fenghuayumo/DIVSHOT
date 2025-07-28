#pragma once
#include <string>

namespace diverse
{
    namespace ImGuiHelper
    {
        struct AssetFileData
        {
            void * data = nullptr;
            size_t dataSize = 0;
        };

        AssetFileData LoadAssetFileData(const char *assetPath);
        void FreeAssetFileData(AssetFileData * assetFileData);

        /**
        @@md#assetFileFullPath

        `std::string AssetFileFullPath(const std::string& assetRelativeFilename)` will return the path to assets.

        This works under all platforms __except Android__.
        For compatibility with Android and other platforms, prefer to use `LoadAssetFileData` whenever possible.

        * Under iOS it will give a path in the app bundle (/private/XXX/....)
        * Under emscripten, it will be stored in the virtual filesystem at "/"
        * Under Android, assetFileFullPath is *not* implemented, and will throw an error:
        assets can be compressed under android, and you cannot use standard file operations!
        Use LoadAssetFileData instead

        @@md
        */
        std::string AssetFileFullPath(const std::string& assetRelativeFilename, bool assertIfNotFound = true);
        inline std::string assetFileFullPath(const std::string& assetRelativeFilename, bool assertIfNotFound = true)
            { return AssetFileFullPath(assetRelativeFilename, assertIfNotFound); }

        // Returns true if this asset file exists
        bool AssetExists(const std::string& assetRelativeFilename);

        extern std::string gAssetsSubfolderFolderName;  // "assets" by default

        // Sets the assets folder location
        // (when using this, automatic assets installation on mobile platforms may not work)
        void SetAssetsFolder(const char* folder);
        void SetAssetsFolder(const std::string& folder);
        void overrideAssetsFolder(const char* folder); // synonym

    } // namespace ImGuiHelper
}  // namespace diverse