#ifndef __TFTP_COMMON_HPP__
#define __TFTP_COMMON_HPP__
#include <boost/asio.hpp>

#include "duration_generator.hpp"
#include "log.hpp"
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
              const ms_duration network_timeout,
              const uint16_t max_retry_count,
              duration_generator_s delay_gen)
      : remote_endpoint(remote_endpoint),
        network_timeout(network_timeout),
        max_retry_count(max_retry_count),
        delay_gen(delay_gen) {
    if (delay_gen == nullptr) {
      this->delay_gen =
          std::make_shared<constant_duration_generator>(ms_duration(CONF_CONSTANT_DELAY_DURATION));
    }
  }

  const udp::endpoint remote_endpoint;
  const ms_duration network_timeout;
  const uint16_t max_retry_count;
  duration_generator_s delay_gen;
};

class base_worker {
public:
  virtual ~base_worker(){};
  virtual void start() = 0;
  virtual void abort() {
    if (this->worker_stage != worker_running) {
      DEBUG("Abort request rejected. Only running worker can be aborted.");
      return;
    }
    this->worker_stage = worker_aborted;
  };

protected:
  base_worker(boost::asio::io_context &io, const base_config &config)
      : remote_endpoint(config.remote_endpoint),
        network_timeout(config.network_timeout),
        max_retry_count(config.max_retry_count),
        delay_gen(config.delay_gen),
        socket(io),
        timer(io),
        delay_timer(io),
        block_number(0),
        retry_count(0),
        worker_stage(worker_constructed) {
    this->socket.open(udp::v4());
  }
  virtual void exit(error_code e) = 0;

  void do_send(const udp::endpoint &endpoint,
               std::function<void(const boost::system::error_code &, const std::size_t)> cb) {
#ifdef CONF_DELAY_SEND
    auto delay = this->delay_gen->get();
    XDEBUG("Executing artificial delay of :%lu ms", delay.count());
    this->delay_timer.expires_after(delay);
    this->delay_timer.async_wait([&, cb](const boost::system::error_code &) {
      this->socket.async_send_to(this->frame->get_asio_buffer(), endpoint, cb);
    });
#else
    this->socket.async_send_to(this->frame->get_asio_buffer(), endpoint, cb);
#endif
  }

  const udp::endpoint remote_endpoint;
  const ms_duration network_timeout;
  const uint16_t max_retry_count;
  duration_generator_s delay_gen;

  udp::socket socket;
  boost::asio::steady_timer timer;
  boost::asio::steady_timer delay_timer;
  uint16_t block_number;
  uint16_t retry_count;
  enum { worker_constructed, worker_running, worker_completed, worker_aborted } worker_stage;
  frame_s frame;
};

} // namespace tftp

#endif
