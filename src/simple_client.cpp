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
  int opt;
  std::string ip, port, work_dir, filename;
  enum { Operation_upload, Operation_download, Operation_none } operation = Operation_none;
  while ((opt = getopt(argc, argv, "H:P:W:D:U:h")) != -1) {
    switch (opt) {
    case 'H':
      ip = std::string(optarg);
      std::cout << "TFTP Server address \t:" << ip << std::endl;
      break;
    case 'P':
      port = std::string(optarg);
      std::cout << "TFTP Server Port \t:" << port << std::endl;
      break;
    case 'W':
      work_dir = std::string(optarg);
      std::cout << "Working directory \t:" << work_dir << std::endl;
      break;
    case 'U':
      operation = Operation_upload;
      filename = std::string(optarg);
      std::cout << "Operation \t\t:Upload\nFile \t\t\t:" << filename << std::endl;
      break;
    case 'D':
      filename = std::string(optarg);
      operation = Operation_download;
      std::cout << "Operation \t\t:Download\nFile \t\t\t:" << filename << std::endl;
      break;
    case '?':
    default:
      std::cout << "Usage :" << argv[0]
                << " -H <server address> -P <port number> -W <working directory> [-D|-U] <filename>" << std::endl;
      return -1;
      break;
    }
  }
  if (ip.empty() || port.empty() || work_dir.empty() || filename.empty() || operation == Operation_none) {
    std::cout << "Invalid arguments. check " << argv[0] << " -h" << std::endl;
    return -1;
  }

  boost::asio::io_context io;
  udp::resolver resolver(io);
  udp::endpoint remote_endpoint;
  remote_endpoint = *resolver.resolve(udp::v4(), ip, port).begin();

  tftp::client_s tftp_client = tftp::client::create(io, remote_endpoint);

  switch (operation) {
  case Operation_download:
    tftp_client->download_file(filename, work_dir + "/" + filename, [=](tftp::error_code error) {
      std::cout << "Download status \t: " << error << std::endl;
    });
    break;
  case Operation_upload:
    tftp_client->upload_file(filename, work_dir + "./" + filename,
                             [=](tftp::error_code error) { std::cout << "Upload status \t\t:" << error << std::endl; });
    break;
  default:
    break;
  }
  io.run();
  std::cout << "-----------------------------------------------------" << std::endl;
  return 0;
}
