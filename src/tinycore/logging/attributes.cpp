//
// Created by yuwenyong on 17-8-4.
//

#include "tinycore/logging/attributes.h"
#include <boost/algorithm/string.hpp>


bool ChannelFilterFactory::isChildOf(const string_type &channel, const string_type &r) {
    if (channel.size() < r.size()) {
        return false;
    } else if (channel.size() == r.size()) {
        return channel == r;
    } else {
        return channel[r.size()] == '.' && boost::starts_with(channel, r);
    }
}