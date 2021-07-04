#ifndef __TFTP_COMMON_HPP__
#define __TFTP_COMMON_HPP__
#include <boost/asio.hpp>

#include "project_config.hpp"
#include "tftp_error_code.hpp"
#include "duration_generator.hpp"

namespace tftp {
using boost::asio::ip::udp;
typedef std::function<void(error_code)> client_completion_callback;

template <typename T>
std::string to_string(const T &endpoint) {
  std::stringstream ss;
  ss << endpoint;
  return ss.str();
}

class base_config {
public:
  base_config(
      const udp::endpoint &remote_endpoint,
      const std::string &remote_file_name,
      const std::string &local_file_name,
      client_completion_callback callback,
      const boost::asio::chrono::duration<uint64_t, std::milli> network_timeout = ms_duration(CONF_NETWORK_TIMEOUT),
      const uint16_t max_retry_count                                            = CONF_MAX_RETRY_COUNT,
      duration_generator_s delay_gen =
          std::make_shared<constant_duration_generator>(ms_duration(CONF_CONSTANT_DELAY_DURATION)))
      : remote_endpoint(remote_endpoint),
        remote_file_name(remote_file_name),
        local_file_name(local_file_name),
        callback(callback),
        network_timeout(network_timeout),
        max_retry_count(max_retry_count),
        delay_gen(delay_gen) {}

  const udp::endpoint remote_endpoint;
  const std::string remote_file_name;
  const std::string local_file_name;
  client_completion_callback callback;
  const boost::asio::chrono::duration<uint64_t, std::micro> network_timeout;
  const uint16_t max_retry_count;
  duration_generator_s delay_gen;
};
} // namespace tftp

#endif
