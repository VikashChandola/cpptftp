#include "client.hpp"
#include "tftp_client.hpp"
#include <boost/asio.hpp>
#include <functional>
#include <iostream>
#include <unistd.h>

class invalid_config_exception : public std::exception {
public:
  invalid_config_exception(const std::string &err_message)
      : std::exception(), message(err_message) {}
  virtual const char *what() const throw() { return this->message.c_str(); }

protected:
  const std::string message;
};

void completion_callback(tftp::error_code status) {
  std::cout << "Completed ["
            << "] status :[" << status << "]" << std::endl;
  return;
}

class execute_conf {
public:
  execute_conf(conf_s c, boost::asio::io_context &io)
      : configuration(c), io(io), running_jobs(0) {
    udp::resolver resolver(io);
    for (client_conf client : this->configuration->client_list) {
      std::cout << "Endpoint :" << client.endpoint << std::endl;
      std::string::size_type separator = client.endpoint.find(":");
      if (separator == std::string::npos) {
        throw invalid_config_exception(std::string("Invalid hostname :") +
                                       client.endpoint);
      }
      std::string ip = client.endpoint.substr(0, separator);
      std::string port = client.endpoint.substr(separator + 1);
      udp::endpoint remote_endpoint;
      try {
        remote_endpoint = *resolver.resolve(udp::v4(), ip, port).begin();
      } catch (...) {
        throw invalid_config_exception(
            std::string("Failed to resolve remote host :") + client.endpoint);
      }
      tftp::client_s tftp_client =
          tftp::client::create(this->io, remote_endpoint);
      std::cout << "Download :" << std::endl;
      std::cout << std::string(40, '-') << std::endl;
      for (job j : client.download_job_list) {
        this->running_jobs++;
        tftp_client->download_file(
            j.remote_file, j.local_file,
            std::bind(&execute_conf::cb, this, std::placeholders::_1,
                      std::string("client [" + j.local_file + " <- " +
                                  j.remote_file + "]")));
        std::cout << "\tlocal file  :" << j.local_file << std::endl;
        std::cout << "\tremote file :" << j.remote_file << std::endl;
        std::cout << "\t" << std::string(32, '-') << std::endl;
      }
      std::cout << std::string(40, '-') << std::endl;

      std::cout << "Upload* :" << std::endl;
      std::cout << std::string(40, '-') << std::endl;
      for (job j : client.upload_job_list) {
        std::cout << "\tlocal file  :" << j.local_file << std::endl;
        std::cout << "\tremote file :" << j.remote_file << std::endl;
        std::cout << "\t" << std::string(32, '-') << std::endl;
      }
      std::cout << std::string(40, '-') << std::endl;
      std::cout << std::endl;
    }
  }

private:
  static void static_cb(tftp::error_code e) {}
  void cb(tftp::error_code e, std::string job_desc) {
    std::cout << job_desc << " completed" << std::endl;
    if (this->running_jobs <= 0) {
      std::cout << "All jobs completed" << std::endl;
    }
    this->running_jobs--;
  }
  conf_s configuration;
  boost::asio::io_context &io;
  int32_t running_jobs;
};

int main(int argc, char **argv) {
  std::string conf_file;
  if (argc < 2) {
    conf_file = std::string("../bin/config.json");
    // std::cout << "Usage " << argv[0] << " <config file>" << std::endl;
    // return 1;
  } else {
    conf_file = std::string(argv[1]);
  }
  conf_s config = std::make_shared<conf>(conf_file);
  boost::asio::io_context io;
  execute_conf(config, io);
  io.run();
  return 0;
}
