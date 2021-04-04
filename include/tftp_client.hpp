#include <array>
#include <boost/asio.hpp>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>

#include "tftp_error_code.hpp"
#include "tftp_frame.hpp"

using boost::asio::ip::udp;

namespace tftp {
typedef void (*client_completion_callback)(tftp::error_code);

class client;
typedef std::shared_ptr<client> client_s;

class client_downloader;
typedef std::shared_ptr<client_downloader> client_downloader_s;

class client_uploader;
typedef std::shared_ptr<client_uploader> client_uploader_s;

class client_downloader
    : public std::enable_shared_from_this<client_downloader> {
public:
  static client_downloader_s
  create(boost::asio::io_context &io, const std::string &file_name,
         const udp::endpoint &remote_endpoint, std::ostream &out_stream,
         client_completion_callback download_callback) {
    client_downloader_s self(new client_downloader(
        io, file_name, remote_endpoint, out_stream, download_callback));
    self->sender(boost::system::error_code(), 0);
    return self;
  }

private:
  void sender(const boost::system::error_code &error,
              const std::size_t bytes_received);

  void receiver(const boost::system::error_code &error,
                const std::size_t bytes_sent);

  void update_stage(const boost::system::error_code &error,
                    const std::size_t bytes_transacted);

  client_downloader(boost::asio::io_context &io, const std::string &file_name,
                    const udp::endpoint &remote_endpoint,
                    std::ostream &out_stream,
                    client_completion_callback download_callback);

  enum read_stage { init, request_data, receive_data, send_ack, exit } stage;

  boost::asio::io_context &io;
  udp::socket socket;
  udp::endpoint remote_tid;
  std::string file_name;
  std::ostream &out;
  client_completion_callback callback;
  tftp_frame_s frame;
  tftp::error_code exec_error;
};

class client_uploader : public std::enable_shared_from_this<client_uploader> {};

class client : public std::enable_shared_from_this<client> {
public:
  static client_s create(boost::asio::io_context &io,
                         const udp::endpoint remote_endpoint) {
    return std::make_shared<client>(client(io, remote_endpoint));
  }

  void download_file(const std::string &file_name, std::string local_file_name,
                     client_completion_callback download_callback) {
    this->download_file(this->io, file_name, this->remote_endpoint, 
                              local_file_name, download_callback);
  }

  void download_file(const std::string &file_name, std::ostream &out_stream,
                     client_completion_callback download_callback) {
    client_downloader::create(this->io, file_name, this->remote_endpoint,
                              out_stream, download_callback);
  }

  void upload_file(const std::string &file_name, std::istream &in_stream,
                   client_completion_callback upload_callback) {
    // client_uploader_s uploader(new client_uploader(
    //    this->io, this->remote_endpoint, in_stream, upload_callback));
  }

private:
  client(boost::asio::io_context &io_, const udp::endpoint &remote_endpoint_)
      : io(io_), remote_endpoint(remote_endpoint_) {}

  boost::asio::io_context &io;
  const udp::endpoint &remote_endpoint;
};

} // namespace tftp
