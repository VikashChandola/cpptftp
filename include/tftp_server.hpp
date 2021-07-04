#ifndef __TFTP_HPP__
#define __TFTP_HPP__

#include <array>
#include <boost/asio.hpp>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>

#include "project_config.hpp"
#include "tftp_error_code.hpp"
#include "tftp_frame.hpp"

using boost::asio::ip::udp;

namespace tftp {

class server_distributor;
typedef std::shared_ptr<server_distributor> server_distributor_s;

class download_server;
typedef std::shared_ptr<download_server> download_server_s;

class upload_server;
typedef std::shared_ptr<upload_server> upload_server_s;

class server {
public:
  server(boost::asio::io_context &io,
         frame_csc &frame,
         const udp::endpoint &endpoint,
         const std::string &work_dir,
         const uint64_t &ms_timeout = CONF_NETWORK_TIMEOUT);

protected:
  static const uint8_t max_retry_count = CONF_MAX_RETRY_COUNT;

  udp::socket socket;
  const udp::endpoint client_endpoint;
  udp::endpoint receive_endpoint;
  std::string filename;
  frame_s frame;
  boost::asio::steady_timer timer;
  const boost::asio::chrono::duration<uint64_t, std::micro> timeout;
  uint16_t block_number;
  bool is_last_frame;
  uint8_t retry_count;
  frame::error_code tftp_error_code;
};

class download_server : public server, public std::enable_shared_from_this<download_server> {
public:
  static void serve(boost::asio::io_context &io,
                    frame_csc &frame,
                    const udp::endpoint &endpoint,
                    const std::string &work_dir);

  download_server(boost::asio::io_context &io,
                  frame_csc &first_frame,
                  const udp::endpoint &endpoint,
                  const std::string &work_dir);

  ~download_server();

private:
  void sender();
  void sender_cb(const boost::system::error_code &e, const std::size_t &bytes_received);
  void receiver();
  void receiver_cb(const boost::system::error_code &e, const std::size_t &bytes_sent);
  bool fill_data_buffer();

  enum download_server_stage {
    ds_send_data,
    ds_resend_data,
    ds_recv_ack,
    ds_send_error,
    ds_recv_timeout,
  };

  download_server_stage stage;
  std::ifstream read_stream;
  char data[TFTP_FRAME_MAX_DATA_LEN];
  std::streamsize data_size;
};

class upload_server : public server, public std::enable_shared_from_this<upload_server> {
public:
  static void serve(boost::asio::io_context &io,
                    frame_csc &frame,
                    const udp::endpoint &endpoint,
                    const std::string &work_dir);

  upload_server(boost::asio::io_context &io,
                frame_csc &first_frame,
                const udp::endpoint &endpoint,
                const std::string &work_dir);

  ~upload_server();

private:
  void sender();
  void sender_cb(const boost::system::error_code &e, const std::size_t &bytes_received);
  void receiver();
  void receiver_cb(const boost::system::error_code &e, const std::size_t &bytes_sent);

  enum upload_server_stage {
    us_send_ack,
    us_resend_ack,
    us_recv_data,
    us_send_error,
    us_recv_timeout,
  };

  upload_server_stage stage;
  std::ofstream write_stream;
};

/* `server_distributor` provides tftp server functionality. server_distributor usage
 * 1. Create server_distributor_s object
 *      server_distributor_s server = server_distributor::create(...)
 * 2. Start listening for connections:
 *      server->start_service()
 *    start_service asynchronously start server. server will continue to run untill stopped.
 * 3. Stop server
 *      server->stop_service()
 *    This call kills server. Currently running download/upload operations will continue to run but no new
 * connections will be accepted. server_distributor object will not hold io context object after stop.
 * server_distributor object acts as a listener and start separate objects(download_server,upload_server) to
 * perform the real operations.
 */
class server_distributor : public std::enable_shared_from_this<server_distributor> {
public:
  /* Creates server_distributor_s object
   * Argument
   * io               :asio io context object
   * local_endpoint   :udp::endpoint on which server will listen for new connection
   * work_dir         :working directory, All file operations will be done relative to this directory.
   * Return : server_distributor_s object
   */
  static server_distributor_s
  create(boost::asio::io_context &io, const udp::endpoint &local_endpoint, std::string work_dir);

  uint64_t stop_service();

  uint64_t start_service();

private:
  void perform_distribution();

  void perform_distribution_cb(const boost::system::error_code &error, const std::size_t &bytes_received);

  server_distributor(boost::asio::io_context &io, const udp::endpoint &local_endpoint, std::string &work_dir);

  // These two data members are forcing server_distributor to accept one connection at a time
  frame_s first_frame;
  udp::endpoint remote_endpoint;

  boost::asio::io_context &io;
  udp::socket socket;
  const std::string work_dir;
  uint64_t server_count;
};

} // namespace tftp

#endif
