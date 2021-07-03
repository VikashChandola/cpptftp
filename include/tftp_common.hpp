#ifndef __TFTP_COMMON_HPP__
#define __TFTP_COMMON_HPP__

#include "async_sleep.hpp"
#include <chrono>

static const ms_duration default_network_timeout(1000);
static const uint16_t default_max_retry_count(3);
#endif
