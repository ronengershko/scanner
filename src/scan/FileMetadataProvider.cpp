#include "scan/FileMetadataProvider.h"
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <sys/stat.h>

FileMetadata FileMetadataProvider::read(const std::filesystem::path& path) const {
    struct stat st;
    if (::stat(path.c_str(), &st) != 0)
        throw std::runtime_error("Cannot stat " + path.string() + ": " + strerror(errno));

    return {
        std::filesystem::weakly_canonical(path),
        static_cast<uint64_t>(st.st_dev),
        static_cast<uint64_t>(st.st_ino),
        static_cast<uintmax_t>(st.st_size),
        static_cast<int64_t>(st.st_mtime)
    };
}

bool FileMetadataProvider::sameFileState(const FileMetadata& first,
                                         const FileMetadata& second) const {
    return first.deviceId        == second.deviceId  &&
           first.inode           == second.inode     &&
           first.size            == second.size      &&
           first.modificationTime == second.modificationTime;
}
