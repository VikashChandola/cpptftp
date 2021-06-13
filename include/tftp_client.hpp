#ifndef __TFTP_CLIENT_HPP__
#define __TFTP_CLIENT_HPP__

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
typedef std::function<void(tftp::error_code)> client_completion_callback;

class client;
typedef std::shared_ptr<client> client_s;

class client_downloader;
typedef std::shared_ptr<client_downloader> client_downloader_s;

class client_uploader;
typedef std::shared_ptr<client_uploader> client_uploader_s;

class client_downloader : public std::enable_shared_from_this<client_downloader> {
public:
  static client_downloader_s create(boost::asio::io_context &io, const std::string &file_name,
                                    const udp::endpoint &remote_endpoint, std::unique_ptr<std::ostream> u_out_stream,
                                    client_completion_callback download_callback);

private:
  void sender(const boost::system::error_code &error, const std::size_t bytes_received);

  void receiver(const boost::system::error_code &error, const std::size_t bytes_sent);

  void update_stage(const boost::system::error_code &error, const std::size_t bytes_transacted);

  client_downloader(boost::asio::io_context &io, const std::string &file_name, const udp::endpoint &remote_endpoint,
                    std::unique_ptr<std::ostream> u_out_stream, client_completion_callback download_callback);

  enum download_stage { init, request_data, receive_data, send_ack, exit } stage;

  udp::socket socket;
  udp::endpoint remote_tid;
  std::string file_name;
  std::unique_ptr<std::ostream> u_out;
  client_completion_callback callback;
  frame_s frame;
  tftp::error_code exec_error;
  bool is_last_block = false;
  boost::asio::steady_timer timer;
  const boost::asio::chrono::duration<uint64_t, std::micro> timeout;
};

class client_uploader : public std::enable_shared_from_this<client_uploader> {
public:
  static client_uploader_s create(boost::asio::io_context &io, const std::string &file_name,
                                  const udp::endpoint &remote_endpoint, std::unique_ptr<std::istream> u_in_stream,
                                  client_completion_callback upload_callback) {
    client_uploader_s self(
        new client_uploader(io, file_name, remote_endpoint, std::move(u_in_stream), upload_callback));
    self->sender(boost::system::error_code(), 0);
    return self;
  }

private:
  void sender(const boost::system::error_code &error, const std::size_t bytes_received);

  void receiver(const boost::system::error_code &error, const std::size_t bytes_sent);

  void update_stage(const boost::system::error_code &error, const std::size_t bytes_transacted);

  client_uploader(boost::asio::io_context &io, const std::string &file_name, const udp::endpoint &remote_endpoint,
                  std::unique_ptr<std::istream> u_in_stream, client_completion_callback download_callback);

  enum upload_stage { init, upload_request, wait_ack, upload_data, exit } stage;

  udp::socket socket;
  udp::endpoint remote_tid;
  std::string file_name;
  std::unique_ptr<std::istream> u_in;
  client_completion_callback callback;
  frame_s frame;
  tftp::error_code exec_error;
  uint16_t block_number;
  bool is_last_block = false;
};

/* `client` class is interface for user to execute tftp client related request. This class allows upload and
 * download of file from remote tftp server. User will have to create one instance of this class per server
 * connection. Usage
 * 1. Create a shared_ptr<client> object
 *      client_s = client::create(...);
 * 2. Execute download_file or upload_file method to download or upload files. Both of these methods take
 *    function object as last argument. This method gets called on completion of download/upload operation.
 */
class client : public std::enable_shared_from_this<client> {
public:
  /* Creates client_s object
   * Argument
   * io               :asio io context object
   * remote_endpoint  :udp::endpoint object for tftp server. All downloads/uploads from this client will be executed on
   *                   this endpoint
   * Return : client_s object
   */
  static client_s create(boost::asio::io_context &io, const udp::endpoint remote_endpoint) {
    return std::make_shared<client>(client(io, remote_endpoint));
  }

  /* Downloads file from tftp server and writes to disk
   * Argument
   * remote_file_name   :file to be downloaded
   * local_file_name    :downloaded file's name
   * download_callback  :callback to be executed on completion of operation
   */
  void download_file(const std::string &remote_file_name, std::string local_file_name,
                     client_completion_callback download_callback) {
    client_downloader::create(this->io, remote_file_name, this->remote_endpoint,
                              std::make_unique<std::ofstream>(local_file_name, std::ios::binary), download_callback);
  }

  void download_file(const std::string &remote_file_name, std::unique_ptr<std::ostream> u_out_stream,
                     client_completion_callback download_callback) {
    client_downloader::create(this->io, remote_file_name, this->remote_endpoint, std::move(u_out_stream),
                              download_callback);
  }

  /* Reads file from disk and uploads to tftp server
   * Argument
   * remote_file_name   :file name to be used in request frame. This is the filename that server will see
   * local_file_name    :file on disk that needs to be uploaded
   * download_callback  :callback to be executed on completion of operation
   */
  void upload_file(const std::string &remote_file_name, std::string local_file_name,
                   client_completion_callback upload_callback) {
    client_uploader::create(this->io, remote_file_name, this->remote_endpoint,
                            std::make_unique<std::ifstream>(local_file_name, std::ios::binary | std::ios::in),
                            upload_callback);
  }
  void upload_file(const std::string &remote_file_name, std::unique_ptr<std::istream> u_in_stream,
                   client_completion_callback upload_callback) {
    client_uploader::create(this->io, remote_file_name, this->remote_endpoint, std::move(u_in_stream), upload_callback);
  }

private:
  client(boost::asio::io_context &io_, const udp::endpoint &remote_endpoint_)
      : io(io_), remote_endpoint(remote_endpoint_) {}

  boost::asio::io_context &io;
  const udp::endpoint remote_endpoint;
};

} // namespace tftp

#endif

/* LEARNING:
 * 1. update_stage method of client_[downloader|uploader] is not specific to sender or receiver. This is problematic
 *    because of stage update may also depend on what was the last thing that was done. Adding another argument to
 *    distiguish caller(sender or receiver) is not going to help either. That will be clubbing to two separate
 *    functionalities in single method. It will be more efficient to have separate update_stage method for sender
 *    and receiver. This way if we want to resend a frame then we don't have to put that logic in
 *    update_stage and receiver(both). Such approach is much uglier than having a sender_cb that calls to sender
 *    directly if resend needed. This approach is used in case of download_server implementaiton.
 *
 * 2. There are multiple exit points in client_downloader class. Exit point is a point from where callback is invoked
 *    It's difficult to keep track this way. If we want to a set of things before exiting then this will have to
 *    be replicated in all the exit points. Secondly such sporadic exits may lead to situation where one forgets to
 *    call callback and user never gets notified. It will be better to have callback invokation from one place. May
 *    be destructor for object or some spefic method.
 * 3. There is no logging in client library. This is not good. It makes first level debugging impossible. Always
 *    add logs with log levels in your program. This way user can get desired amount of logs. This was addressed to
 *    some extent in server implementation. There it's only cout and cerr but better than nothing.
 */
