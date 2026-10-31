#pragma once
#include "types.h"
inline bool saveLastKnownTime(time_t) { return true; }
inline time_t loadLastKnownTime() { return 0; }
