#include "client.hpp"
#include "tftp_client.hpp"
#include <boost/asio.hpp>
#include <iostream>
#include <string>

#include "tftp_error_code.hpp"
#include "tftp_frame.hpp"

void cb(const std::string &filename, tftp::error_code e) {
  std::cout << "Done file name :" << filename << ", error_code :" << e << std::endl;
}

int main(int argc, char **argv) {
  (void)(argc);
  (void)(argv);
  boost::asio::io_context io;
  std::string ip(argv[1]);
  std::string port("12345");
  udp::resolver resolver(io);
  udp::endpoint remote_endpoint;
  remote_endpoint = *resolver.resolve(udp::v4(), ip, port).begin();

  tftp::client_s tftp_client = tftp::client::create(io, remote_endpoint);
  std::string filename_base("sample_");
  for (int i = 0; i < 32; i++) {
    std::string filename = filename_base + std::to_string(i);
    std::string local_filename = std::string("./client_dir/") + std::string("simple_client_") + filename;
    tftp_client->download_file(filename, local_filename, [=](tftp::error_code error) {
      std::cout << "Download status for " << filename << " status " << error << std::endl;
      if (error != 0) {
        std::cout << "Not reversing" << std::endl;
        return;
      }
      tftp_client->upload_file(local_filename, local_filename, [=](tftp::error_code error) {
        std::cout << "Reversed " << local_filename << " Status :" << error << std::endl;
      });
    });
  }
  // tftp_client->download_file("0010_file", "0010_file", cb);
  // tftp_client->download_file("0100_file", "0100_file", cb);
  // tftp_client->download_file("1000_file", "1000_file", cb);
  // tftp_client->upload_file("simple_client", "simple_client", cb);
  io.run();
  return 0;
}
