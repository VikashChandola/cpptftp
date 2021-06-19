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

#include "tftp_common.hpp"
#include "tftp_error_code.hpp"
#include "tftp_frame.hpp"

using boost::asio::ip::udp;

namespace tftp {
typedef std::function<void(tftp::error_code)> client_completion_callback;

class client;
typedef std::shared_ptr<client> client_s;

class base_client;

class client_config;

class download_client_config;
class download_client;
typedef std::shared_ptr<download_client> download_client_s;

class client_uploader_config;
class client_uploader;
typedef std::shared_ptr<client_uploader> client_uploader_s;

class client_config {
public:
  client_config(
      const udp::endpoint &remote_endpoint,
      const std::string &work_dir,
      const std::string &remote_file_name,
      const std::string &local_file_name,
      client_completion_callback callback,
      const boost::asio::chrono::duration<uint64_t, std::milli> network_timeout = default_network_timeout,
      const uint16_t retry_count                                                = default_retry_count)
      : remote_endpoint(remote_endpoint),
        work_dir(work_dir),
        remote_file_name(remote_file_name),
        local_file_name(local_file_name),
        callback(callback),
        network_timeout(network_timeout),
        retry_count(retry_count) {}

  const udp::endpoint remote_endpoint;
  const std::string work_dir;
  const std::string remote_file_name;
  const std::string local_file_name;
  client_completion_callback callback;
  const boost::asio::chrono::duration<uint64_t, std::micro> network_timeout;
  const uint16_t retry_count;
};

class download_client_config : public client_config {
public:
  using client_config::client_config;
};

class base_client {
public:
  virtual ~base_client();
  virtual void start();
  virtual void stop();

protected:
  base_client(boost::asio::io_context &io, const client_config &config);
  virtual void sender()                                                                              = 0;
  virtual void sender_cb(const boost::system::error_code &error, const std::size_t bytes_sent)       = 0;
  virtual void receiver()                                                                            = 0;
  virtual void receiver_cb(const boost::system::error_code &error, const std::size_t bytes_received) = 0;
  virtual void exit(tftp::error_code e);

  const udp::endpoint server_endpoint;
  const std::string work_dir;
  const std::string remote_file_name;
  const std::string local_file_name;
  client_completion_callback callback;
  const boost::asio::chrono::duration<uint64_t, std::micro> timeout;
  const uint16_t retry_count;

  udp::socket socket;
  udp::endpoint server_tid;
  udp::endpoint receive_tid;
  frame_s frame;
  boost::asio::steady_timer timer;
  uint16_t block_number;
  enum { client_constructed, client_running, client_completed, client_aborted } client_stage;
  std::fstream file_handle;
};

class download_client : public std::enable_shared_from_this<download_client>, public base_client {
public:
  static download_client_s create(boost::asio::io_context &io, const download_client_config &config);
  ~download_client(){};
  void start();
  void stop();

private:
  download_client(boost::asio::io_context &io, const download_client_config &config);
  void sender();
  void sender_cb(const boost::system::error_code &error, const std::size_t bytes_sent);
  void receiver();
  void receiver_cb(const boost::system::error_code &error, const std::size_t bytes_received);

  template <typename T>
  bool write(T itr, const T &itr_end) noexcept {
    if (!this->is_file_open) {
      this->file_handle  = std::fstream(this->work_dir + this->local_file_name, std::ios::out);
      this->is_file_open = true;
    }
    if (!this->file_handle.is_open()) {
      return false;
    }
    while (itr != itr_end) {
      this->file_handle << *itr;
    }
    return true;
  }

  enum { dc_request_data, dc_receive_data, dc_send_ack, dc_resend_ack, dc_abort } download_stage;
  // indicator that now we are on last block of transaction
  bool is_last_block;
  // indicated whether file is opened. This is needed for lazy file opening
  bool is_file_open;
};

class client_uploader : public std::enable_shared_from_this<client_uploader> {
public:
  static client_uploader_s create(boost::asio::io_context &io,
                                  const std::string &file_name,
                                  const udp::endpoint &remote_endpoint,
                                  std::unique_ptr<std::istream> u_in_stream,
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

  client_uploader(boost::asio::io_context &io,
                  const std::string &file_name,
                  const udp::endpoint &remote_endpoint,
                  std::unique_ptr<std::istream> u_in_stream,
                  client_completion_callback download_callback);

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
class client {
public:
  /* Creates client_s object
   * Argument
   * io               :asio io context object
   * remote_endpoint  :udp::endpoint object for tftp server. All downloads/uploads from this client will be
   * executed on this endpoint Return : client_s object
   */
  static client_s create(boost::asio::io_context &io,
                         const udp::endpoint remote_endpoint,
                         const std::string work_dir = "./") {
    return std::make_shared<client>(client(io, remote_endpoint, work_dir));
  }

  /* Downloads file from tftp server and writes to disk
   * Argument
   * remote_file_name   :file to be downloaded
   * local_file_name    :downloaded file's name
   * download_callback  :callback to be executed on completion of operation
   */
  void download_file(const std::string &remote_file_name,
                     const std::string &local_file_name,
                     client_completion_callback download_callback) {
    download_client_config config(this->remote_endpoint,
                                  this->work_dir,
                                  remote_file_name,
                                  local_file_name,
                                  download_callback);
    auto worker = download_client::create(this->io, config);
    worker->start();
  }

  /* Reads file from disk and uploads to tftp server
   * Argument
   * remote_file_name   :file name to be used in request frame. This is the filename that server will see
   * local_file_name    :file on disk that needs to be uploaded
   * download_callback  :callback to be executed on completion of operation
   */
  void upload_file(const std::string &remote_file_name,
                   std::string local_file_name,
                   client_completion_callback upload_callback) {
    client_uploader::create(this->io,
                            remote_file_name,
                            this->remote_endpoint,
                            std::make_unique<std::ifstream>(local_file_name, std::ios::binary | std::ios::in),
                            upload_callback);
  }
  void upload_file(const std::string &remote_file_name,
                   std::unique_ptr<std::istream> u_in_stream,
                   client_completion_callback upload_callback) {
    client_uploader::create(this->io,
                            remote_file_name,
                            this->remote_endpoint,
                            std::move(u_in_stream),
                            upload_callback);
  }

private:
  client(boost::asio::io_context &io_, const udp::endpoint &remote_endpoint_, const std::string work_dir_)
      : io(io_),
        remote_endpoint(remote_endpoint_),
        work_dir(work_dir_) {}

  boost::asio::io_context &io;
  const udp::endpoint remote_endpoint;
  const std::string work_dir;
};

} // namespace tftp

#endif

/* LEARNING:
 * 1. update_stage method of client_[downloader|uploader] is not specific to sender or receiver. This is
 * problematic because of stage update may also depend on what was the last thing that was done. Adding
 * another argument to distiguish caller(sender or receiver) is not going to help either. That will be
 * clubbing to two separate functionalities in single method. It will be more efficient to have separate
 * update_stage method for sender and receiver. This way if we want to resend a frame then we don't have to
 * put that logic in update_stage and receiver(both). Such approach is much uglier than having a sender_cb
 * that calls to sender directly if resend needed. This approach is used in case of download_server
 * implementaiton.
 *
 * 2. There are multiple exit points in download_client class. Exit point is a point from where callback is
 * invoked It's difficult to keep track this way. If we want to a set of things before exiting then this will
 * have to be replicated in all the exit points. Secondly such sporadic exits may lead to situation where one
 * forgets to call callback and user never gets notified. It will be better to have callback invokation from
 * one place. May be destructor for object or some spefic method.
 * 3. There is no logging in client library. This is not good. It makes first level debugging impossible.
 * Always add logs with log levels in your program. This way user can get desired amount of logs. This was
 * addressed to some extent in server implementation. There it's only cout and cerr but better than nothing.
 */
