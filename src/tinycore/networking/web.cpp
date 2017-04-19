//
// Created by yuwenyong on 17-4-17.
//

#include "tinycore/networking/web.h"
#include "tinycore/debugging/trace.h"


const char* HTTPError::what() const noexcept {
    if (_what.empty()) {
        _what += _file;
        _what += ':';
        _what += std::to_string(_line);
        _what += " in ";
        _what += _func;
        _what += ' ';
        _what += getTypeName();
        _what += "\n\t";
        _what += "HTTP ";
        _what += std::to_string(static_cast<int>(_statusCode));
        _what += " ";
        ASSERT(HTTPResponses.find(_statusCode) != HTTPResponses.end());
        _what += HTTPResponses[_statusCode];
        std::string what = std::runtime_error::what();
        if (!what.empty()) {
            _what += " (";
            _what += what;
            _what += ")";
        }
    }
    return _what.c_str();
}


const StringSet GZipContentEncoding::CONTENT_TYPES = {
        "text/plain",
        "text/html",
        "text/css",
        "text/xml",
        "application/x-javascript",
        "application/xml",
        "application/atom+xml",
        "text/javascript",
        "application/json",
        "application/xhtml+xml"
};

GZipContentEncoding::GZipContentEncoding(HTTPRequestPtr request)
        : _gzipFile(_gzipValue) {
    if (request->supportsHTTP1_1()) {
        _gzipping = true;
    } else {
        auto header = request->getHTTPHeader();
        std::string acceptEncoding = header->getDefault("Accept-Encoding");
        _gzipping = acceptEncoding.find("gzip") != std::string::npos;
    }
}

void GZipContentEncoding::transformFirstChunk(StringMap &headers, std::vector<char> &chunk, bool finishing) {
    if (_gzipping) {
        auto iter = headers.find("Content-Type");
        std::string contentType = iter != headers.end() ? iter->second : "";
        auto pos = contentType.find(';');
        if (pos == std::string::npos) {
            ThrowException(IndexError, "list index out of range");
        }
        std::string ctype = contentType.substr(0, pos);
        _gzipping = CONTENT_TYPES.find(ctype) != CONTENT_TYPES.end()
                    && (!finishing || chunk.size() >= MIN_LENGTH)
                    && (finishing || headers.find("Content-Length") == headers.end())
                    && headers.find("Content-Encoding") == headers.end();
    }
    if (_gzipping) {
        headers["Content-Encoding"] = "gzip";
    }
}

void GZipContentEncoding::transformChunk(std::vector<char> &chunk, bool finishing) {
    if (_gzipping) {
        _gzipFile.write(chunk);
        if (finishing) {
            _gzipFile.close();
        } else {
            _gzipFile.flush();
        }
        chunk.assign(std::next(_gzipValue.begin(), _gzipPos), _gzipValue.end());
        _gzipPos = _gzipValue.size();
    }
}


void ChunkedTransferEncoding::transformFirstChunk(StringMap &headers, std::vector<char> &chunk, bool finishing) {
    if (_chunking) {
        if (headers.find("Content-Length") != headers.end() || headers.find("Transfer-Encoding") != headers.end()) {
            _chunking = false;
        } else {
            headers["Transfer-Encoding"] = "chunked";
            transformChunk(chunk, finishing);
        }
    }
}

void ChunkedTransferEncoding::transformChunk(std::vector<char> &chunk, bool finishing) {
    if (_chunking) {
        std::string block;
        if (!chunk.empty()) {
            block = String::format("%x\r\n");
            chunk.insert(chunk.begin(), block.begin(), block.end());
            block = "\r\n";
            chunk.insert(chunk.end(), block.begin(), block.end());
        }
        if (finishing) {
            block = "0\r\n\r\n";
            chunk.insert(chunk.end(), block.begin(), block.end());
        }
    }
}