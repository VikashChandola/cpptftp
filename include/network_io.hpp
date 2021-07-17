#ifndef __NETWORK_IO_HPP__
#define __NETWORK_IO_HPP__

#include <boost/asio.hpp>

#include "duration_generator.hpp"
#include "log.hpp"
#include "utility.hpp"

namespace nio {

using boost::asio::ip::udp;

typedef std::function<void(const boost::system::error_code &, const std::size_t &)> completion_cb;

/* receive objects can be used for receiving data from remote endpoint.
 * Life of this object must be  preserved by caller. This class doesn't do self life management.
 * socket needs to be created by caller and maintained to valid while this object is in use.
 */
class receiver final {
public:
  receiver(boost::asio::io_context &io, udp::socket &socket, const ms_duration &timeout)
      : socket(socket),
        timer(io),
        network_timeout(timeout) {}

  void async_receive(boost::asio::mutable_buffer &asio_buffer, completion_cb callback) {
    XDEBUG("Receiving on local endpoint :%s", to_string(this->socket.local_endpoint()).c_str());
    this->socket.async_receive_from(
        asio_buffer,
        this->remote_endpoint,
        std::bind(&receiver::callback_transit, this, callback, std::placeholders::_1, std::placeholders::_2));
    this->timer.expires_after(this->network_timeout);
    this->timer.async_wait([=](const boost::system::error_code &error) {
      (void)(callback);
      if (error == boost::asio::error::operation_aborted) {
        return;
      }
      this->socket.cancel();
    });
  }

  const udp::endpoint &get_receive_endpoint() { return this->remote_endpoint; }

private:
  void callback_transit(completion_cb callback,
                        const boost::system::error_code &error,
                        const std::size_t &bytes_received) {
    this->timer.cancel();
    callback(error, bytes_received);
  }
  udp::socket &socket;
  boost::asio::steady_timer timer;
  ms_duration network_timeout;
  udp::endpoint remote_endpoint;
};

class sender final {
public:
  sender(boost::asio::io_context &io, udp::socket &socket, duration_generator_s delay_gen)
      : socket(socket),
        delay_timer(io),
        delay_gen(delay_gen) {}

  void async_send(boost::asio::mutable_buffer &asio_buffer, const udp::endpoint &endpoint, completion_cb cb) {
    auto delay = this->delay_gen->get();
    XDEBUG("Executing artificial delay of :%lu ms", delay.count());
    this->delay_timer.expires_after(delay);
    this->delay_timer.async_wait(
        [=](const boost::system::error_code &) { this->socket.async_send_to(asio_buffer, endpoint, cb); });
  }

private:
  udp::socket &socket;
  boost::asio::steady_timer delay_timer;
  duration_generator_s delay_gen;
};

} // namespace nio
#endif
