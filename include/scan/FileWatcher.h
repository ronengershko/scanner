#pragma once
#include <CoreServices/CoreServices.h>
#include <functional>
#include <string>
#include <vector>

using FileChangedCallback = std::function<void(const std::string& path)>;

class FileWatcher {
public:
    FileWatcher(const std::vector<std::string>& paths, FileChangedCallback cb);
    ~FileWatcher();
    bool start();  // returns true=reload (SIGUSR1), false=shutdown (SIGTERM/SIGINT)

private:
    static void fsCallback(ConstFSEventStreamRef, void* ctx,
                           size_t count, void* paths,
                           const FSEventStreamEventFlags[],
                           const FSEventStreamEventId[]);

    std::vector<std::string> m_paths;
    FileChangedCallback      m_callback;
    FSEventStreamRef         m_stream = nullptr;
};
