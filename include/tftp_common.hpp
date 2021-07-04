#ifndef __TFTP_COMMON_HPP__
#define __TFTP_COMMON_HPP__
#include <boost/asio.hpp>

#include "duration_generator.hpp"
#include "project_config.hpp"
#include "tftp_error_code.hpp"
#include "tftp_frame.hpp"

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
  base_config(const udp::endpoint &remote_endpoint,
              const std::string &remote_file_name,
              const std::string &local_file_name,
              client_completion_callback callback,
              const boost::asio::chrono::duration<uint64_t, std::milli> network_timeout =
                  ms_duration(CONF_NETWORK_TIMEOUT),
              const uint16_t max_retry_count = CONF_MAX_RETRY_COUNT,
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

class base_worker {
public:
  virtual ~base_worker(){};
  virtual void start() = 0;
  virtual void abort() = 0;

protected:
  base_worker(boost::asio::io_context &io, const base_config &config)
      : server_endpoint(config.remote_endpoint),
        remote_file_name(config.remote_file_name),
        local_file_name(config.local_file_name),
        callback(config.callback),
        timeout(config.network_timeout),
        max_retry_count(config.max_retry_count),
        delay_gen(config.delay_gen),
        socket(io),
        timer(io),
        delay_timer(io),
        block_number(0),
        client_stage(client_constructed),
        retry_count(0) {
    this->socket.open(udp::v4());
  }
  virtual void exit(error_code e) = 0;

  void do_send(const udp::endpoint &endpoint,
               std::function<void(const boost::system::error_code &, const std::size_t)> cb) {
#ifdef CONF_DELAY_SEND
    this->delay_timer.expires_after(this->delay_gen->get());
    this->delay_timer.async_wait([&, cb](const boost::system::error_code &) {
      this->socket.async_send_to(this->frame->get_asio_buffer(), endpoint, cb);
    });
#else
    this->socket.async_send_to(this->frame->get_asio_buffer(), endpoint, cb);
#endif
  }

  const udp::endpoint server_endpoint;
  const std::string remote_file_name;
  const std::string local_file_name;
  client_completion_callback callback;
  const boost::asio::chrono::duration<uint64_t, std::micro> timeout;
  const uint16_t max_retry_count;
  duration_generator_s delay_gen;
  uint16_t window_size = 512;

  udp::socket socket;
  udp::endpoint server_tid;
  udp::endpoint receive_tid;
  frame_s frame;
  boost::asio::steady_timer timer;
  boost::asio::steady_timer delay_timer;
  uint16_t block_number;
  enum { client_constructed, client_running, client_completed, client_aborted } client_stage;
  std::fstream file_handle;
  uint16_t retry_count;
};

} // namespace tftp

#endif
