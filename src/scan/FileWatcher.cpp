#include "scan/FileWatcher.h"
#include <csignal>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

static volatile sig_atomic_t g_reload = 0;

static void sigusr1Handler(int) {
    g_reload = 1;
}

void FileWatcher::fsCallback(ConstFSEventStreamRef, void* ctx,
                              size_t count, void* paths,
                              const FSEventStreamEventFlags[],
                              const FSEventStreamEventId[]) {
    auto* self = static_cast<FileWatcher*>(ctx);
    auto** filePaths = static_cast<char**>(paths);
    for (size_t i = 0; i < count; ++i) {
        fs::path p(filePaths[i]);
        if (fs::is_regular_file(p))
            self->m_callback(p.string());
    }
}

FileWatcher::FileWatcher(const std::vector<std::string>& paths, FileChangedCallback cb)
    : m_paths(paths), m_callback(std::move(cb)) {}

FileWatcher::~FileWatcher() {
    if (m_stream) {
        FSEventStreamStop(m_stream);
        FSEventStreamInvalidate(m_stream);
        FSEventStreamRelease(m_stream);
    }
}

void FileWatcher::start() {
    std::vector<CFStringRef> cfStrings;
    cfStrings.reserve(m_paths.size());
    for (const auto& p : m_paths)
        cfStrings.push_back(CFStringCreateWithCString(nullptr, p.c_str(), kCFStringEncodingUTF8));

    CFArrayRef cfPaths = CFArrayCreate(nullptr,
                                       reinterpret_cast<const void**>(cfStrings.data()),
                                       static_cast<CFIndex>(cfStrings.size()),
                                       &kCFTypeArrayCallBacks);

    FSEventStreamContext ctx = {0, this, nullptr, nullptr, nullptr};
    m_stream = FSEventStreamCreate(nullptr,
                                   &FileWatcher::fsCallback,
                                   &ctx,
                                   cfPaths,
                                   kFSEventStreamEventIdSinceNow,
                                   0.5,
                                   kFSEventStreamCreateFlagFileEvents |
                                   kFSEventStreamCreateFlagNoDefer);
    CFRelease(cfPaths);
    for (auto s : cfStrings) CFRelease(s);

    FSEventStreamScheduleWithRunLoop(m_stream, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    FSEventStreamStart(m_stream);

    for (const auto& p : m_paths)
        std::cout << "Monitoring: " << p << "\n";
    std::cout << "(Ctrl+C to stop)\n";

    signal(SIGUSR1, sigusr1Handler);
    g_reload = 0;

    // Run in 1-second ticks so we can check the reload flag
    while (!g_reload)
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
}
