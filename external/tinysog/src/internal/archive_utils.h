/* SPDX-FileCopyrightText: 2025 TinySOG Authors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#pragma once


#include "tinysog/expected.h"
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace tinysog {
    namespace internal {

        /**
         * @brief ZIP archive writer
         */
        class ArchiveWriter {
        public:
            ArchiveWriter(const std::filesystem::path& output_path);
            ~ArchiveWriter();

            // Disable copy
            ArchiveWriter(const ArchiveWriter&) = delete;
            ArchiveWriter& operator=(const ArchiveWriter&) = delete;

            /**
             * @brief Add file to archive
             */
            bool add_file(const std::string& filename,
                          const void* data,
                          size_t size);

        private:
            void* archive_ = nullptr;  // libarchive handle
            std::filesystem::path path_;
        };

        /**
         * @brief Read all files from ZIP archive
         *
         * @param archive_path Archive path
         * @return Mapping from filename to data
         */
        tinysog::internal::expected<std::map<std::string, std::vector<uint8_t>>, std::string>
        read_archive(const std::filesystem::path& archive_path);

        /**
         * @brief Read all files from directory
         *
         * @param dir_path Directory path
         * @param filenames List of filenames to read
         * @return Mapping from filename to data
         */
        tinysog::internal::expected<std::map<std::string, std::vector<uint8_t>>, std::string>
        read_directory(const std::filesystem::path& dir_path,
                       const std::vector<std::string>& filenames);

    } // namespace internal
} // namespace tinysog

