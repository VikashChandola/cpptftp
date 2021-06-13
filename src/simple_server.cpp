#include <boost/asio.hpp>
#include <iostream>
#include <string>
#include <unistd.h>

#include "tftp_error_code.hpp"
#include "tftp_server.hpp"

void cb(tftp::error_code e) { std::cout << "Done error_code :" << e << std::endl; }

int main(int argc, char **argv) {
  int opt;
  std::string ip, port, work_dir;
  while ((opt = getopt(argc, argv, "H:P:W:h")) != -1) {
    switch (opt) {
    case 'H':
      ip = std::string(optarg);
      std::cout << "Host address \t\t:" << ip << std::endl;
      break;
    case 'P':
      port = std::string(optarg);
      std::cout << "Host Port \t\t:" << port << std::endl;
      break;
    case 'W':
      work_dir = std::string(optarg);
      std::cout << "Working directory \t:" << work_dir << std::endl;
      break;
    case '?':
    default:
      std::cout << "Usage :" << argv[0] << " -H <host address> -P <port number> -W <working directory>" << std::endl;
      return -1;
      break;
    }
  }
  if (ip.empty() || port.empty() || work_dir.empty()) {
    std::cout << "Invalid arguments. check " << argv[0] << " -h" << std::endl;
    return -1;
  }

  boost::asio::io_context io;
  udp::resolver resolver(io);
  udp::endpoint local_endpoint;
  local_endpoint = *resolver.resolve(udp::v4(), ip, port).begin();
  tftp::distributor_s tftp_server = tftp::distributor::create(io, local_endpoint, work_dir);
  tftp_server->start_service();
  io.run();
  return 0;
}
