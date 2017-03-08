//
// Created by yuwenyong on 17-3-6.
//

#include "tinycore/logging/appenderconsole.h"
#include <boost/core/null_deleter.hpp>


AppenderConsole::SinkTypePtr AppenderConsole::_createSink() const {
    using SinkBackend = sinks::text_ostream_backend;
    using SinkFrontend = sinks::synchronous_sink<SinkBackend>;
    auto backend = boost::make_shared<SinkBackend >();
    backend->add_stream(boost::shared_ptr<std::ostream>(&std::clog, boost::null_deleter()));
    backend->auto_flush(true);
    auto sink = boost::make_shared<SinkFrontend>(backend);
    return sink;
}