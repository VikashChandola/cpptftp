#include <boost/asio.hpp>
#include <iostream>
#include <string>

#include "tftp_server.hpp"
#include "tftp_error_code.hpp"

void cb(tftp::error_code e) { std::cout << "Done error_code :" << e << std::endl; }

int main(int argc, char **argv) {
  (void)(argc);
  (void)(argv);
  boost::asio::io_context io;
  std::string ip("127.0.0.1");
  std::string port("12345");
  udp::resolver resolver(io);
  udp::endpoint local_endpoint;
  local_endpoint = *resolver.resolve(udp::v4(), ip, port).begin();

  /*udp::socket socket(io);
  socket.open(udp::v4());
  std::array<char, 1> send_buf  = {{ 0 }};
  socket.async_send_to(boost::asio::buffer(send_buf), local_endpoint, cb);*/

  tftp::distributor_s tftp_server = tftp::distributor::create(io, local_endpoint, "work_dir");
  tftp_server->start_service();
  // tftp_client->download_file("0010_file", "0010_file", cb);
  // tftp_client->download_file("0100_file", "0100_file", cb);
  // tftp_client->download_file("1000_file", "1000_file", cb);
  //tftp_client->upload_file("simple_client", "simple_client", cb);
  io.run();
  return 0;
}
