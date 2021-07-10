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
#include "tftp_common.hpp"
#include "tftp_error_code.hpp"
#include "tftp_frame.hpp"

using boost::asio::ip::udp;

namespace tftp {

class server_distributor;
typedef std::shared_ptr<server_distributor> server_distributor_s;

class server;

class server_config;

class download_server;
typedef std::shared_ptr<download_server> download_server_s;

class upload_server;
typedef std::shared_ptr<upload_server> upload_server_s;

class server_config : public base_config {
public:
  frame_csc frame;
  std::string filename;
  server_config(const udp::endpoint &remote_endpoint,
                const std::string &work_dir,
                frame_csc &frame,
                const ms_duration network_timeout = ms_duration(CONF_NETWORK_TIMEOUT),
                const uint16_t max_retry_count    = CONF_MAX_RETRY_COUNT,
                duration_generator_s delay_gen =
                    std::make_shared<constant_duration_generator>(ms_duration(CONF_CONSTANT_DELAY_DURATION)))
      : base_config(remote_endpoint, network_timeout, max_retry_count, delay_gen),
        frame(frame),
        filename(work_dir + "/" + frame->get_filename()) {}
};

class download_server_config : public server_config {
public:
  using server_config::server_config;
};

class upload_server_config : public server_config {
public:
  using server_config::server_config;
};

class server : public base_worker {
public:
  server(boost::asio::io_context &io, const server_config &config);

protected:
  udp::endpoint receive_endpoint;
  std::string filename;
  bool is_last_frame;
  frame::error_code tftp_error_code;
  enum { server_constructed, server_running, server_completed, server_aborted } server_stage;
};

class download_server : public server, public std::enable_shared_from_this<download_server> {
public:
  static download_server_s create(boost::asio::io_context &io, const download_server_config &config);

  void start() override;
  void abort() override;
  void exit(error_code e) override { (void)(e); };

  ~download_server() override;

private:
  download_server(boost::asio::io_context &io, const download_server_config &config);

  void send_data(const bool &resend = false);
  void send_error(const frame::error_code &, const std::string &message = "");
  void receive_ack();
  void receive_ack_cb(const boost::system::error_code &error, const std::size_t &bytes_received);

  bool fill_data_buffer();

  std::ifstream read_stream;
  char data[TFTP_FRAME_MAX_DATA_LEN];
  std::streamsize data_size;
};

class upload_server : public server, public std::enable_shared_from_this<upload_server> {
public:
  static upload_server_s create(boost::asio::io_context &io, const upload_server_config &config);

  ~upload_server();
  void start() override;
  void abort() override{};
  void exit(error_code e) override { (void)(e); };

private:
  upload_server(boost::asio::io_context &io, const upload_server_config &config);
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
