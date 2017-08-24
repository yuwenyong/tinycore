//
// Created by yuwenyong on 17-8-16.
//

#include "tinycore/common/random.h"


std::default_random_engine Random::_engine((unsigned int)time(nullptr));
