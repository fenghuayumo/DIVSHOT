/* SPDX-FileCopyrightText: 2025 TinySOG Authors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "archive_utils.h"
#include <archive.h>
#include <archive_entry.h>
#include <chrono>
#include <cstddef>
#include <fstream>

#include "tinysog/expected.h"
#ifdef _WIN32
using ssize_t = std::ptrdiff_t;
#endif

namespace tinysog {
    namespace internal {

        ArchiveWriter::ArchiveWriter(const std::filesystem::path& output_path)
            : path_(output_path) {

            archive_ = archive_write_new();
            auto* a = static_cast<struct archive*>(archive_);
            archive_write_set_format_zip(a);
            archive_write_open_filename(a, path_.string().c_str());
        }

        ArchiveWriter::~ArchiveWriter() {
            if (archive_) {
                auto* a = static_cast<struct archive*>(archive_);
                archive_write_close(a);
                archive_write_free(a);
            }
        }

        bool ArchiveWriter::add_file(
            const std::string& filename,
            const void* data,
            size_t size) {

            struct archive_entry* entry = archive_entry_new();

            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);

            archive_entry_set_pathname(entry, filename.c_str());
            archive_entry_set_size(entry, size);
            archive_entry_set_filetype(entry, AE_IFREG);
            archive_entry_set_perm(entry, 0644);
            archive_entry_set_mtime(entry, time_t, 0);

            auto* a = static_cast<struct archive*>(archive_);
            if (archive_write_header(a, entry) != ARCHIVE_OK) {
                archive_entry_free(entry);
                return false;
            }

            if (archive_write_data(a, data, size) != static_cast<ssize_t>(size)) {
                archive_entry_free(entry);
                return false;
            }

            archive_entry_free(entry);
            return true;
        }

        tinysog::internal::expected<std::map<std::string, std::vector<uint8_t>>, std::string>
        read_archive(const std::filesystem::path& archive_path) {

            struct archive* a = archive_read_new();
            archive_read_support_format_zip(a);
            archive_read_support_filter_all(a);

            if (archive_read_open_filename(a, archive_path.string().c_str(), 10240) != ARCHIVE_OK) {
                std::string error = archive_error_string(a);
                archive_read_free(a);
                return tinysog::internal::unexpected("Failed to open archive: " + error);
            }

            std::map<std::string, std::vector<uint8_t>> files;
            struct archive_entry* entry;

            while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
                std::string filename = archive_entry_pathname(entry);
                size_t size = archive_entry_size(entry);

                std::vector<uint8_t> data(size);
                ssize_t r = archive_read_data(a, data.data(), size);

                if (r != static_cast<ssize_t>(size)) {
                    archive_read_free(a);
                    return tinysog::internal::unexpected("Failed to read file from archive: " + filename);
                }

                files[filename] = std::move(data);
            }

            archive_read_free(a);
            return files;
        }

        tinysog::internal::expected<std::map<std::string, std::vector<uint8_t>>, std::string>
        read_directory(
            const std::filesystem::path& dir_path,
            const std::vector<std::string>& filenames) {

            if (!std::filesystem::exists(dir_path)) {
                return tinysog::internal::unexpected("Directory does not exist: " + dir_path.string());
            }

            std::map<std::string, std::vector<uint8_t>> files;

            for (const auto& filename : filenames) {
                auto file_path = dir_path / filename;

                if (!std::filesystem::exists(file_path)) {
                    return tinysog::internal::unexpected("File not found: " + file_path.string());
                }

                std::ifstream file(file_path, std::ios::binary);
                if (!file) {
                    return tinysog::internal::unexpected("Failed to open file: " + file_path.string());
                }

                file.seekg(0, std::ios::end);
                size_t size = file.tellg();
                file.seekg(0, std::ios::beg);

                std::vector<uint8_t> data(size);
                file.read(reinterpret_cast<char*>(data.data()), size);

                if (!file) {
                    return tinysog::internal::unexpected("Failed to read file: " + file_path.string());
                }

                files[filename] = std::move(data);
            }

            return files;
        }

    } // namespace internal
} // namespace tinysog

