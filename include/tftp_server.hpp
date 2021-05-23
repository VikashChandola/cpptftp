#ifndef __TFTP_HPP__
#define __TFTP_HPP__

#include <boost/asio.hpp>
#include <array>
#include <boost/asio.hpp>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>

#include "tftp_frame.hpp"
#include "tftp_error_code.hpp"

using boost::asio::ip::udp;

namespace tftp {

class distributor;
typedef std::shared_ptr<distributor> distributor_s;

class download_server;
typedef std::shared_ptr<download_server> download_server_s;

class server {
public:
  server(boost::asio::io_context &io, frame_csc &frame, const udp::endpoint &endpoint,
         const std::string &work_dir, uint64_t ms_timeout = 1000);

protected:
  udp::socket socket;
  const udp::endpoint client_endpoint;
  udp::endpoint receive_endpoint;
  std::string filename;
  frame_s frame;
  boost::asio::steady_timer timer;
  const boost::asio::chrono::duration<uint64_t, std::micro> timeout;
};

class download_server : public server, public std::enable_shared_from_this<download_server> {
public:
  static void serve(boost::asio::io_context &io, frame_csc &frame, const udp::endpoint &endpoint,
                    const std::string &work_dir);

  download_server(boost::asio::io_context &io, frame_csc &first_frame, const udp::endpoint &endpoint,
                  const std::string &work_dir);

  ~download_server();
private:

  void sender();

  void sender_cb(const boost::system::error_code &e, const std::size_t &bytes_received);

  bool fill_data_buffer();

  void receiver();

  void receiver_cb(const boost::system::error_code &e, const std::size_t &bytes_sent);

  void update_stage(const boost::system::error_code &e,const std::size_t &bytes_transacted);

  enum download_server_stage{
    ds_init,
    ds_send_data,
    ds_resend_data,
    ds_recv_ack,
    ds_send_error,
    ds_recv_timeout,
    ds_exit
  };

  static const uint8_t retry_count = 3;

  download_server_stage stage;
  std::ifstream read_stream;
  std::uint16_t block_number;
  bool is_last_frame;
  char data[TFTP_FRAME_MAX_DATA_LEN];
  std::streamsize data_size;
  uint8_t resend_count;
};

class distributor : public std::enable_shared_from_this<distributor> {
public:

  static distributor_s create(boost::asio::io_context &io, const udp::endpoint &local_endpoint,
                              std::string work_dir);

  static distributor_s create(boost::asio::io_context &io, const uint16_t udp_port, std::string work_dir);

  uint64_t stop_service();

  uint64_t start_service();

private:

  void perform_distribution();

  void perform_distribution_cb(const boost::system::error_code &error, const std::size_t &bytes_received);

  distributor(boost::asio::io_context &io, const udp::endpoint &local_endpoint, std::string &work_dir);

  //These two data members are forcing distributor to accept one connection at a time
  frame_s first_frame;
  udp::endpoint remote_endpoint;

  boost::asio::io_context &io;
  udp::socket socket;
  const std::string work_dir;
  uint64_t server_count;
};

} //namespace tftp

#endif
