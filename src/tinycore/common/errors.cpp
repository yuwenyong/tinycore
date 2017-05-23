//
// Created by yuwenyong on 17-3-31.
//

#include "tinycore/common/errors.h"


const char* SystemExit::what() const {
    if (_what.empty()) {
        _what += _file;
        _what += ':';
        _what += std::to_string(_line);
        _what += " in ";
        _what += _func;
        _what += ' ';
        _what += " SystemExit";
        if (!_message.empty()) {
            _what += "\n\t";
            _what += _message;
        }
    }
    return _what.c_str();
}


const char* BaseException::what() const noexcept {
    if (_what.empty()) {
        _what += _file;
        _what += ':';
        _what += std::to_string(_line);
        _what += " in ";
        _what += _func;
        _what += ' ';
        _what += getTypeName();
        _what += "\n\t";
        _what += std::runtime_error::what();
    }
    return _what.c_str();
}


const char* AssertionError::what() const noexcept {
    if (_what.empty()) {
        _what += _file;
        _what += ':';
        _what += std::to_string(_line);
        _what += " in ";
        _what += _func;
        _what += ' ';
        _what += getTypeName();
        _what += "\n\t";
        _what += std::runtime_error::what();
        if (!_extra.empty()) {
            _what += "\n\t";
            _what += _extra;
        }
    }
    return _what.c_str();
}