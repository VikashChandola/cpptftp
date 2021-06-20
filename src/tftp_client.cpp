#include <array>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>

#include <boost/asio.hpp>

#include "log.hpp"
#include "tftp_client.hpp"
#include "tftp_error_code.hpp"
#include "tftp_exception.hpp"

using boost::asio::ip::udp;
using namespace tftp;

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

base_client::base_client(boost::asio::io_context &io, const client_config &config)
    : server_endpoint(config.remote_endpoint),
      remote_file_name(config.remote_file_name),
      local_file_name(config.local_file_name),
      callback(config.callback),
      timeout(config.network_timeout),
      max_retry_count(config.max_retry_count),
      socket(io),
      timer(io),
      block_number(0),
      client_stage(client_constructed),
      retry_count(0) {
  this->socket.open(udp::v4());
}

base_client::~base_client() {}

void base_client::exit(error_code e) {
  this->timer.cancel();
  this->socket.close();
  this->file_handle.close();
  this->callback(e);
}

//-----------------------------------------------------------------------------

download_client::download_client(boost::asio::io_context &io, const download_client_config &config)
    : base_client(io, config),
      download_stage(dc_request_data),
      is_last_block(false),
      is_file_open(false) {
  DEBUG("Setting up client to download remote file [%s] from [%s] to [%s]",
        this->remote_file_name.c_str(),
        to_string(this->server_endpoint).c_str(),
        this->local_file_name.c_str());
  if (this->server_tid.port() != 0 && this->receive_tid.port() != 0) {
    /* server_tid and receive_tid must be initialized with 0 by default constructors otherwise it won't be
     * possible to verify remote endpoint(refer receiver_cb method). Current constructors of asio initialize
     * with port as 0.
     */
    ERROR("server tids are not zero. client can not function");
    exit(1);
  }
}

download_client_s download_client::create(boost::asio::io_context &io, const download_client_config &config) {
  download_client_s self(new download_client(io, config));
  return self;
}

void download_client::start() {
  if (this->client_stage == client_constructed) {
    this->client_stage   = client_running;
    this->download_stage = dc_request_data;
    this->sender();
    DEBUG("Downloading file [%s]", this->local_file_name.c_str());
  } else {
    DEBUG("download has alraedy started.");
  }
}

void download_client::abort() {
  this->client_stage   = client_aborted;
  this->download_stage = dc_abort;
  this->socket.cancel();
}

void download_client::exit(error_code e) {
  this->download_stage = dc_complete;
  base_client::exit(e);
}

void download_client::sender() {
  mark;
  switch (this->download_stage) {
  case dc_request_data: {
    this->frame = frame::create_read_request_frame(this->remote_file_name);
    this->socket.async_send_to(this->frame->get_asio_buffer(),
                               this->server_endpoint,
                               std::bind(&download_client::sender_cb,
                                         shared_from_this(),
                                         std::placeholders::_1,
                                         std::placeholders::_2));
    return;
  }
  case dc_send_ack: {
    this->frame = frame::create_ack_frame(this->block_number);
    this->socket.async_send_to(this->frame->get_asio_buffer(),
                               this->server_tid,
                               std::bind(&download_client::sender_cb,
                                         shared_from_this(),
                                         std::placeholders::_1,
                                         std::placeholders::_2));
    return;
  }
    return;
  case dc_receive_data:
  case dc_complete:
    break;
  case dc_abort:
    this->sender_cb(boost::system::error_code(), 0);
    return;
  }
  ERROR("FATAL ERROR: download client state machine reached invalid stage");
  this->exit(error::state_machine_broke);
}

void download_client::sender_cb(const boost::system::error_code &error, const std::size_t bytes_sent) {
  (void)(bytes_sent);
  mark;
  if (error) {
    ERROR("%s[%d] Received unrecoverable error [%s]",
          __func__,
          __LINE__,
          base_client::to_string(error).c_str());
    this->exit(error::boost_asio_error_base + error.value());
    return;
  }
  switch (this->download_stage) {
  case dc_request_data: {
    this->download_stage = dc_receive_data;
    this->receiver();
    return;
  }
  case dc_send_ack: {
    if (this->is_last_block) {
      this->download_stage = dc_complete;
      this->client_stage   = client_completed;
      // This is the only valid good exit.
      this->exit(error::no_error);
      return;
    }
    this->download_stage = dc_receive_data;
    this->receiver();
    return;
  }
  case dc_abort: {
    this->abort();
    this->exit(error::connection_aborted);
    return;
  }
  case dc_complete:
  case dc_receive_data:
    break;
  }
  ERROR("FATAL ERROR: download client state machine reached invalid stage");
  this->exit(error::state_machine_broke);
}

void download_client::receiver() {
  switch (this->download_stage) {
  case dc_receive_data:
    this->frame = frame::create_empty_frame();
    this->socket.async_receive_from(this->frame->get_asio_buffer(),
                                    this->receive_tid,
                                    std::bind(&download_client::receiver_cb,
                                              shared_from_this(),
                                              std::placeholders::_1,
                                              std::placeholders::_2));
    this->timer.expires_after(this->timeout);
    this->timer.async_wait([&](const boost::system::error_code &error) {
      if (error == boost::asio::error::operation_aborted) {
        return;
      }
      this->socket.cancel();
    });
    return;
  case dc_request_data:
  case dc_send_ack:
    break;
  case dc_abort:
  case dc_complete:
    this->receiver_cb(boost::system::error_code(), 0);
    return;
  }
  ERROR("FATAL ERROR: download client state machine reached invalid stage");
  this->exit(error::state_machine_broke);
}

void download_client::re_send(const error_code &current_error) {
  this->retry_count += 1;
  if (this->retry_count >= this->max_retry_count) {
    this->exit(current_error);
  } else {
    this->sender();
  }
}

void download_client::re_receive(const error_code &current_error) {
  this->retry_count += 1;
  if (this->retry_count >= this->max_retry_count) {
    this->exit(current_error);
  } else {
    this->receiver();
  }
}

void download_client::receiver_cb(const boost::system::error_code &error, const std::size_t bytes_received) {
  (void)(bytes_received);
  bool timed_out = false;
  this->timer.cancel();
  if (error == boost::asio::error::operation_aborted) {
    timed_out = true;
  } else if (error) {
    ERROR("%s[%d] Received unrecoverable error [%s]",
          __func__,
          __LINE__,
          base_client::to_string(error).c_str());
    this->exit(error::boost_asio_error_base + error.value());
    return;
  }
  switch (this->download_stage) {
  case dc_receive_data: {
    if (timed_out) {
      WARN("Connection to %s timed out while waiting for response", to_string(this->receive_tid).c_str());
      // Probably last packet didn't reach upto server
      this->download_stage = dc_send_ack;
      this->re_send(error::receive_timeout);
      return;
    }
    // Is remote point same as it was previously, If not is_server_endpoint_good have taken care of next steps
    if (!this->is_server_endpoint_good()) {
      return;
    }
    // Is frame proper. If it's not is_frame_type_data have taken care of next steps
    this->frame->resize(bytes_received);
    if (!this->is_frame_type_data()) {
      return;
    }
    // parse whatever we have received from server and update local file and block number accordingly
    if (!parse_received_data()) {
      return;
    }
    this->download_stage = dc_send_ack;
    this->sender();
    return;
  }
  case dc_complete:
  case dc_abort:
    this->client_stage = client_aborted;
    this->exit(error::no_error);
    return;
  case dc_send_ack:
  case dc_request_data:
    break;
  }
  ERROR("FATAL ERROR: download client state machine reached invalid stage");
  this->exit(error::state_machine_broke);
}

//-----------------------------------------------------------------------------

client_uploader::client_uploader(boost::asio::io_context &io,
                                 const std::string &file_name,
                                 const udp::endpoint &remote_endpoint,
                                 std::unique_ptr<std::istream> u_in_stream,
                                 client_completion_callback upload_callback)
    : socket(io),
      remote_tid(remote_endpoint),
      file_name(file_name),
      u_in(std::move(u_in_stream)),
      callback(upload_callback),
      exec_error(0),
      block_number(0) {
  this->socket.open(udp::v4());
  this->stage = init;
}

void client_uploader::sender(const boost::system::error_code &error, const std::size_t bytes_received) {
  // std::cout << this->remote_tid << " [" << __func__ << ":" << __LINE__ << "] "
  //          << " Stage :" << this->stage << std::endl;

  this->update_stage(error, bytes_received);
  switch (this->stage) {
  case client_uploader::upload_request: {
    this->frame = frame::create_write_request_frame(this->file_name);
    this->socket.async_send_to(this->frame->get_asio_buffer(),
                               this->remote_tid,
                               std::bind(&client_uploader::receiver,
                                         shared_from_this(),
                                         std::placeholders::_1,
                                         std::placeholders::_2));
    break;
  }
  case client_uploader::upload_data: {
    try {
      this->frame->parse_frame();
    } catch (framing_exception &e) {
      // std::cout << this->remote_tid << " [" << __func__ << "] Failed to parse response from " <<
      // std::endl;
      this->callback(error::invalid_server_response);
      return;
    }
    switch (this->frame->get_op_code()) {
    case frame::op_ack:
      break;
    case frame::op_error:
      std::cerr << this->remote_tid << " [" << __func__
                << "]  Server responded with error :" << this->frame->get_error_code() << std::endl;
      this->callback(this->frame->get_error_code());
      return;
    default:
      std::cerr << this->remote_tid << " [" << __func__
                << "]  Server responded with unknown opcode :" << this->frame->get_op_code() << std::endl;
      this->callback(error::invalid_server_response);
      return;
    }
    char data_array[512] = {0};
    u_in->read(data_array, 512);
    // std::vector<char> data_vector(data_array);
    std::vector<char> data_vector;
    for (int i = 0; i < u_in->gcount(); i++) {
      data_vector.push_back(data_array[i]);
    }
    if (u_in->eof()) {
      this->is_last_block = true;
    }
    this->frame = frame::create_data_frame(data_vector.cbegin(), data_vector.cend(), this->block_number);
    this->socket.async_send_to(this->frame->get_asio_buffer(),
                               this->remote_tid,
                               std::bind(&client_uploader::receiver,
                                         shared_from_this(),
                                         std::placeholders::_1,
                                         std::placeholders::_2));
    break;
  }
  case client_uploader::exit: {
    this->callback(this->exec_error);
    return;
  }
  default:
    std::cerr << this->remote_tid << " [" << __func__ << "] Stage :" << this->stage
              << " state machine reached invalid stage." << std::endl;
    break;
  }
}

void client_uploader::receiver(const boost::system::error_code &error, const std::size_t bytes_sent) {
  // std::cout << this->remote_tid << " [" << __func__ << ":" << __LINE__ << "] "
  //          << " Stage :" << this->stage << std::endl;
  this->update_stage(error, bytes_sent);
  switch (this->stage) {
  case client_uploader::wait_ack: {
    this->frame = frame::create_empty_frame();
    this->socket.async_receive_from(this->frame->get_asio_buffer(),
                                    this->remote_tid,
                                    std::bind(&client_uploader::sender,
                                              shared_from_this(),
                                              std::placeholders::_1,
                                              std::placeholders::_2));
    break;
  }
  case client_uploader::exit: {
    this->callback(this->exec_error);
    return;
  }
  default: {
    std::cerr << this->remote_tid << " [" << __func__ << "] Stage :" << this->stage
              << " state machine reached invalid stage." << std::endl;
    break;
  }
  }
}

void client_uploader::update_stage(const boost::system::error_code &error,
                                   const std::size_t bytes_transacted) {
  // TODO: Add necessary changes for errornous case
  (void)(error);
  (void)(bytes_transacted);
  switch (this->stage) {
  case client_uploader::init: {
    this->stage = client_uploader::upload_request;
    break;
  }
  case client_uploader::upload_request: {
    this->stage = client_uploader::wait_ack;
    break;
  }
  case client_uploader::wait_ack: {
    this->block_number++;
    if (this->is_last_block) {
      this->stage = client_uploader::exit;
    } else {
      this->stage = client_uploader::upload_data;
    }
    break;
  }
  case client_uploader::upload_data: {
    this->stage = client_uploader::wait_ack;
    break;
  }
  case client_uploader::exit:
  default: {
    break;
  }
  }
}

//-----------------------------------------------------------------------------
