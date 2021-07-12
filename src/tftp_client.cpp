#include <array>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>

#include <boost/asio.hpp>

#include "duration_generator.hpp"
#include "log.hpp"
#include "tftp_client.hpp"
#include "tftp_error_code.hpp"
#include "tftp_exception.hpp"

using boost::asio::ip::udp;
using namespace tftp;

//-----------------------------------------------------------------------------

download_client::download_client(boost::asio::io_context &io, const download_client_config &config)
    : client(io, config),
      remote_file_name(config.remote_file_name),
      local_file_name(config.local_file_name),
      callback(config.callback),
      client_stage(client_constructed),
      is_last_block(false),
      is_file_open(false) {
  DEBUG("Setting up client to download remote file [%s] from [%s] to [%s]",
        this->remote_file_name.c_str(),
        to_string(this->remote_endpoint).c_str(),
        this->local_file_name.c_str());
  if (this->server_tid.port() != 0 && this->receive_tid.port() != 0) {
    /* server_tid and receive_tid must be initialized with 0 by default constructors otherwise it won't be
     * possible to verify remote endpoint(refer receiver_cb method). Current constructors of asio initialize
     * with port as 0. This assert is to ensure that any future change in default constructor gets identified
     * quickly.
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
  if (this->client_stage != client_constructed) {
    NOTICE("Start request rejected. Start can only be requested for freshly constructed objects.");
    return;
  }
  this->client_stage = client_running;
  DEBUG("Downloading file %s from server hosted at %s",
        this->local_file_name.c_str(),
        to_string(this->remote_endpoint).c_str());
  this->send_request();
}

void download_client::abort() {
  if (this->client_stage != client_running) {
    DEBUG("Abort request reject. Only running client can aborted.");
    return;
  }
  this->client_stage = client_aborted;
  // this->socket.cancel();
}

void download_client::exit(error_code e) {
  if (this->client_stage != client_completed) {
    this->client_stage = client_completed;
  } else {
    ERROR("Exit rejected. There can be only one exit");
    return;
  }
  this->callback(e);
}

void download_client::send_request() {
  XDEBUG("Sending request to download file %s", this->remote_file_name.c_str());
  this->frame = frame::create_read_request_frame(this->remote_file_name);
  this->do_send(this->remote_endpoint,
                std::bind(&download_client::send_request_cb,
                          shared_from_this(),
                          std::placeholders::_1,
                          std::placeholders::_2));
}

void download_client::send_request_cb(const boost::system::error_code &error, const std::size_t bytes_sent) {
  (void)(bytes_sent);
  if (error) {
    ERROR("%s[%d] send failed Error :%s", __func__, __LINE__, to_string(error).c_str());
    this->exit(error::boost_asio_error_base + error.value());
  } else {
    this->receive_data();
  }
}

void download_client::send_ack() { this->send_ack_for_block_number(this->block_number); }

void download_client::send_ack_for_block_number(uint16_t block_num) {
  XDEBUG("Sending ack for block number %u", block_num);
  this->frame = frame::create_ack_frame(block_num);
  this->do_send(this->server_tid,
                std::bind(&download_client::send_ack_cb,
                          shared_from_this(),
                          std::placeholders::_1,
                          std::placeholders::_2));
}

void download_client::send_ack_cb(const boost::system::error_code &error, const std::size_t bytes_sent) {
  (void)(bytes_sent);
  if (error) {
    ERROR("%s[%d] Received unrecoverable error [%s]", __func__, __LINE__, to_string(error).c_str());
    this->exit(error::boost_asio_error_base + error.value());
    return;
  }
  if (this->is_last_block) {
    this->exit(error::no_error);
  } else {
    this->receive_data();
  }
}

void download_client::receive_data() {
  XDEBUG("Waiting for block number %u", this->block_number + 1);
  this->frame = frame::create_empty_frame();
  this->socket.async_receive_from(this->frame->get_asio_buffer(),
                                  this->receive_tid,
                                  std::bind(&download_client::receive_data_cb,
                                            shared_from_this(),
                                            std::placeholders::_1,
                                            std::placeholders::_2));
  this->timer.expires_after(this->network_timeout);
  this->timer.async_wait([&](const boost::system::error_code &e) {
    if (e == boost::asio::error::operation_aborted) {
      return;
    }
    this->socket.cancel();
  });
}

void download_client::receive_data_cb(const boost::system::error_code &error,
                                      const std::size_t bytes_received) {
  if (this->client_stage == client_aborted) {
    // Abort is happening because of user request
    this->exit(error::user_requested_abort);
    return;
  }
  if (error == boost::asio::error::operation_aborted) {
    XDEBUG("Receive timed out for block number %u", this->block_number + 1);
    WARN("Timed out while waiting for response on %s", to_string(this->socket.local_endpoint()).c_str());
    if (this->retry_count++ >= this->max_retry_count) {
      this->exit(error::receive_timeout);
      return;
    } else {
      if (this->block_number == 0) {
        this->send_request();
      } else {
        this->send_ack();
      }
    }
    return;
  }
  if (this->server_tid.port() == 0) {
    this->server_tid = this->receive_tid;
  } else if (this->server_tid != this->receive_tid) {
    WARN("Expecting data from %s but received from %s",
         to_string(this->server_tid).c_str(),
         to_string(this->receive_tid).c_str());
    if (this->retry_count++ >= this->max_retry_count) {
      this->exit(error::invalid_server_response);
      return;
    } else {
      this->receive_data();
    }
    return;
  }
  std::pair<std::vector<char>::const_iterator, std::vector<char>::const_iterator> itr_pair;
  try {
    this->frame->resize(bytes_received);
    this->frame->parse_frame();
    if (this->frame->get_op_code() != frame::op_data) {
      WARN("Server responded with error code :%u, message :%s",
           this->frame->get_error_code(),
           this->frame->get_error_message().c_str());
      this->exit(this->frame->get_error_code());
      return;
    }
    itr_pair = this->frame->get_data_iterator();
  } catch (framing_exception &e) {
    // This could happen on packet fragmentation or bad packet
    WARN("Failed to parse response from %s", to_string(this->receive_tid).c_str());
    this->receive_data();
    return;
  }
  XDEBUG("Received block number %u", this->frame->get_block_number());
  if (this->block_number + 1 != this->frame->get_block_number()) {
    WARN("Expected blocks number %u got %u", this->block_number + 1, this->frame->get_block_number());
    WARN("Block %u is rejected", this->frame->get_block_number());
    // May be last ack didn't reach upto remote end
    this->send_ack_for_block_number(this->frame->get_block_number());
    return;
  }
  this->block_number = this->frame->get_block_number();
  if (!this->write(itr_pair.first, itr_pair.second)) {
    // Failed to write to file. Fatal error
    ERROR("IO Error on file %s. Aborting download", this->local_file_name.c_str());
    this->exit(error::disk_io_error);
    return;
  }
  this->retry_count = 0;
  if (itr_pair.second - itr_pair.first != this->window_size) {
    this->is_last_block = true;
  }
  this->send_ack();
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
