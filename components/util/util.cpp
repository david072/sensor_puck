#include "util.h"

ulong millis() { return esp_timer_get_time() / 1000; }
