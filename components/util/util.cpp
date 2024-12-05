#include "util.h"

u32 millis() { return esp_timer_get_time() / 1000; }
