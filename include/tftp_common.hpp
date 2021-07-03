#ifndef __TFTP_COMMON_HPP__
#define __TFTP_COMMON_HPP__

#include "async_sleep.hpp"
#include "project_config.hpp"

static const ms_duration default_network_timeout(CONF_NETWORK_TIMEOUT);
static const uint16_t default_max_retry_count(CONF_MAX_RETRY_COUNT);

template <typename T>
std::string to_string(const T &endpoint) {
  std::stringstream ss;
  ss << endpoint;
  return ss.str();
}
#endif
