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

#include "duration_generator.hpp"
#include "file_io.hpp"
#include "log.hpp"
#include "project_config.hpp"
#include "tftp_common.hpp"
#include "tftp_error_code.hpp"
#include "tftp_frame.hpp"

using boost::asio::ip::udp;

namespace tftp {
typedef std::function<void(error_code)> client_completion_callback;

class client_distributor;
typedef std::shared_ptr<client_distributor> client_distributor_s;

class client;

class client_config;

class download_client_config;
class download_client;
typedef std::shared_ptr<download_client> download_client_s;

class upload_client_config;
class upload_client;
typedef std::shared_ptr<upload_client> upload_client_s;

class client_config : public base_config {
public:
  client_config(const udp::endpoint &remote_endpoint,
                const std::string &remote_file_name,
                const std::string &local_file_name,
                client_completion_callback callback,
                const ms_duration network_timeout = ms_duration(CONF_NETWORK_TIMEOUT),
                const uint16_t max_retry_count    = CONF_MAX_RETRY_COUNT,
                duration_generator_s delay_gen    = nullptr)
      : base_config(remote_endpoint, network_timeout, max_retry_count, delay_gen),
        remote_file_name(remote_file_name),
        local_file_name(local_file_name),
        callback(callback) {}
  const std::string remote_file_name;
  const std::string local_file_name;
  client_completion_callback callback;
};

class download_client_config : public client_config {
public:
  using client_config::client_config;
};

class upload_client_config : public client_config {
public:
  using client_config::client_config;
};

class client : public base_worker {
public:
  client(boost::asio::io_context &io, const client_config &config)
      : base_worker(io, config),
        remote_file_name(config.remote_file_name),
        local_file_name(config.local_file_name),
        callback(config.callback) {
    if (this->server_tid.port() != 0 && this->receive_tid.port() != 0) {
      /* server_tid and receive_tid must be initialized with 0 by default constructors otherwise it won't be
       * possible to verify remote endpoint(refer receiver_x_cb method). Current constructors of asio
       * initialize with port as 0. This assert is to ensure that any future change in default constructor
       * gets identified quickly.
       */
      ERROR("server tids are not zero. client can not function");
      std::exit(1);
    }
  }

protected:
  const std::string remote_file_name;
  const std::string local_file_name;
  client_completion_callback callback;
  udp::endpoint server_tid;
  udp::endpoint receive_tid;
};

class download_client : public std::enable_shared_from_this<download_client>, public client {
public:
  static download_client_s create(boost::asio::io_context &io, const download_client_config &config);
  ~download_client(){};
  void start() override;

private:
  download_client(boost::asio::io_context &io, const download_client_config &config);
  void send_request();
  void send_request_cb(const boost::system::error_code &error, const std::size_t bytes_sent);
  void send_ack();
  void send_ack_for_block_number(uint16_t);
  void send_ack_cb(const boost::system::error_code &error, const std::size_t bytes_sent);
  void receive_data();
  void receive_data_cb(const boost::system::error_code &error, const std::size_t bytes_received);
  void exit(error_code e) override;

  // indicator that now we are on last block of transaction
  bool is_last_block;
  fileio::writer write_handle;
};

class upload_client : public std::enable_shared_from_this<upload_client> {
public:
  static upload_client_s create(boost::asio::io_context &io,
                                const std::string &file_name,
                                const udp::endpoint &remote_endpoint,
                                std::unique_ptr<std::istream> u_in_stream,
                                client_completion_callback upload_callback) {
    upload_client_s self(
        new upload_client(io, file_name, remote_endpoint, std::move(u_in_stream), upload_callback));
    self->sender(boost::system::error_code(), 0);
    return self;
  }

private:
  void sender(const boost::system::error_code &error, const std::size_t bytes_received);

  void receiver(const boost::system::error_code &error, const std::size_t bytes_sent);

  void update_stage(const boost::system::error_code &error, const std::size_t bytes_transacted);

  upload_client(boost::asio::io_context &io,
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
  error_code exec_error;
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
class client_distributor {
public:
  /* Creates client_distributor_s object
   * Argument
   * io               :asio io context object
   * remote_endpoint  :udp::endpoint object for tftp server. All downloads/uploads from this client will be
   * executed on this endpoint Return : client_s object
   */
  static client_distributor_s create(boost::asio::io_context &io,
                                     const udp::endpoint remote_endpoint,
                                     const std::string work_dir = "./") {
    client_distributor_s self(new client_distributor(io, remote_endpoint, work_dir));
    return self;
    // How the fuck was this not crashing ?
    // One free by shared_ptr other by stack ??
    // return std::make_shared<client_distributor>(client_distributor(io, remote_endpoint, work_dir));
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
                                  remote_file_name,
                                  this->work_dir + local_file_name,
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
    upload_client::create(this->io,
                          remote_file_name,
                          this->remote_endpoint,
                          std::make_unique<std::ifstream>(local_file_name, std::ios::binary | std::ios::in),
                          upload_callback);
  }

private:
  client_distributor(boost::asio::io_context &io_,
                     const udp::endpoint &remote_endpoint_,
                     const std::string work_dir_)
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
 * ---------------------------------------------06/20/2021---------------------------------------------
 * 4. Looping between sender, sender_cb and receiver, receiver_cb is fine as long of number of states is
 * limited. This is getting compilcated as number of states increases. Basically for every new state all of
 * these four methods need to be aware on what to do. Even if we say don't handle this case and abort. That's
 * not right way. There are only a known set of things that sender_cb need to do when a read request is
 * sent(that is to check for transmition correctness and ask receiver to listen) but right now code to handle
 * other things is also clubbed in same function. Smaller blocks will give clearer code flow.
 *
 * 5. Don't maintain multiple multiple states. It is just against the dry priciples. client_state and
 * download_state. Both are somewhat similar. Does it worth to keep such copies ???
 *
 * 6. Don't make a base class just for the sake of it. There isn't much base_client(now client) is serving at
 * this point of time ???
 *
 * 7. Always maintain one or max two parllel task at hand. Anything more than that becomes a complicated
 * jugglery. Some places where two tasks are unavoidable is, when we are receiving some data with timeout.
 * Here both operaitons will have to fired parlley. There is no other way. (May be async_receive methods could
 * have helped with timeout arguments here)
 * ---------------------------------------------07/14/2021---------------------------------------------
 * 8. Whole design was top to bottom code reuse with inheritance. This has lead to classical horizontal
 * code reuse problem. Code composition should have been taken into design consideration ie stratergy pattern.
 * download_client and upload_server shares a lot of code but they can't share it since it has to be placed in
 * base_worker which is not right place.
 */
