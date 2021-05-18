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

class server;
typedef std::unique_ptr<server> server_u;

class download_server;
typedef std::unique_ptr<download_server> download_server_u;

class server {
public:
  server(boost::asio::io_context &io, frame_csc &frame, const udp::endpoint &endpoint);

protected:
  udp::socket socket;
  udp::endpoint client_endpoint;
  std::string filename;
  frame_s frame;
};

class download_server : public server {
public:
  static void serve(boost::asio::io_context &io, frame_csc &frame, const udp::endpoint &endpoint);

  download_server(boost::asio::io_context &io, frame_csc &first_frame, const udp::endpoint &endpoint);

  ~download_server(){std::cout <<"Destructor" << std::endl;}
private:

  //Eventhough sender and receiver are getting ds_u are reference they are owner of this object and rightly
  //handover it's ownership to asio via bind or let it die
  static void sender(download_server_u &ds_u, const boost::system::error_code &e, std::size_t bytes_received);

  static void receiver(download_server_u &ds_u, const boost::system::error_code &e, std::size_t bytes_sent);

  void update_stage(const std::size_t &bytes_transacted);

  enum download_server_stage{
    ds_init,
    ds_send_data,
    ds_recv_ack,
    ds_exit
  };
  download_server_stage stage;
  std::ifstream read_stream;
  std::uint16_t block_number;
  bool is_last_frame;
  char data[513];
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
