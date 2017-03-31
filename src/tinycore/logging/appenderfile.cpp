//
// Created by yuwenyong on 17-3-6.
//

#include "tinycore/logging/appenderfile.h"
#include <boost/filesystem.hpp>
#include "tinycore/logging/log.h"
#include "tinycore/common/errors.h"


AppenderFile::AppenderFile(std::string name, LogLevel level, AppenderFlags flags, const StringVector &extraArgs)
        : Appender(name, level, flags) {
    if (extraArgs.empty()) {
        std::string error;
        error = "Missing file name for appender" + name;
        ThrowException(IllegalArguments, error);
    }
    _fileName = extraArgs[0];
    if (extraArgs.size() > 1) {
        if (extraArgs[1] == "w") {
            _mode = std::ios_base::trunc | std::ios_base::out;
        }
    }
    if (extraArgs.size() > 2) {
        _maxFileSize = static_cast<size_t>(std::stoi(extraArgs[2]));
    }
    size_t dotPos = _fileName.find_last_of(".");
    if (dotPos != std::string::npos) {
        _fileName.insert(dotPos, "_%Y%m%d_%4N");
    } else {
        _fileName += "_%Y%m%d_%4N";
    }
    boost::filesystem::path logDir(Log::getLogsDir());
    logDir /= _fileName;
    _fileName = logDir.string();
    std::cout << "fileName:" << _fileName << std::endl;
}

Appender::SinkTypePtr AppenderFile::_createSink() const {
    using SinkBackend = sinks::text_file_backend;
    using SinkFrontend = sinks::synchronous_sink<SinkBackend>;
    auto backend = boost::make_shared<SinkBackend >(
            keywords::file_name = _fileName,
            keywords::open_mode = _mode,
            keywords::rotation_size = _maxFileSize,
            keywords::time_based_rotation = sinks::file::rotation_at_time_point(0, 0, 0),
            keywords::auto_flush = true
    );
//    backend->set_file_collector(sinks::file::make_collector(
//            keywords::target = "logs",
//    keywords::max_size = 16 * 1024 * 1024,
//    keywords::min_free_space = 100 * 1024 * 1024,
//    keywords::max_files = 512
//    ));
//    backend->scan_for_files(sinks::file::scan_all);
//    backend->auto_flush(true);
    auto sink = boost::make_shared<SinkFrontend>(backend);
    return sink;
}