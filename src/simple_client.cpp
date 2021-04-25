#include "client.hpp"
#include "tftp_client.hpp"
#include <boost/asio.hpp>
#include <iostream>
#include <string>

#include "tftp_error_code.hpp"
#include "tftp_frame.hpp"

void recv_cb(boost::system::error_code e, std::size_t bytes_transferred) {
  std::cout << "Done" << std::endl;
}

void cb(tftp::error_code e) { std::cout << "Done" << std::endl; }

int main(int argc, char **argv) {

  boost::asio::io_context io;
  std::string ip("127.0.0.1");
  std::string port("12345");
  udp::resolver resolver(io);
  udp::endpoint remote_endpoint;
  remote_endpoint = *resolver.resolve(udp::v4(), ip, port).begin();

  /*udp::socket socket(io);
  socket.open(udp::v4());
  std::array<char, 1> send_buf  = {{ 0 }};
  socket.async_send_to(boost::asio::buffer(send_buf), remote_endpoint, cb);*/

  tftp::client_s tftp_client = tftp::client::create(io, remote_endpoint);
  // tftp_client->download_file("0001_file", "0001_file", cb);
  // tftp_client->download_file("0010_file", "0010_file", cb);
  // tftp_client->download_file("0100_file", "0100_file", cb);
  // tftp_client->download_file("1000_file", "1000_file", cb);
  tftp_client->upload_file("simple_client", "simple_client", cb);
  io.run();
  return 0;
}
