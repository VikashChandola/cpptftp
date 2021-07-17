#include <array>
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
      is_last_block(false),
      write_handle(config.local_file_name),
      receiver(io, this->socket, config.network_timeout),
      sender(io, this->socket, config.delay_gen) {
  DEBUG("Setting up client to download remote file [%s] from [%s] to [%s]",
        this->remote_file_name.c_str(),
        to_string(this->remote_endpoint).c_str(),
        this->local_file_name.c_str());
}

download_client_s download_client::create(boost::asio::io_context &io, const download_client_config &config) {
  download_client_s self(new download_client(io, config));
  return self;
}

void download_client::start() {
  if (this->get_stage() != worker_constructed) {
    NOTICE("Start request rejected. Start can only be requested for freshly constructed objects.");
    return;
  }
  this->set_stage_running();
  DEBUG("Downloading file %s from server hosted at %s",
        this->local_file_name.c_str(),
        to_string(this->remote_endpoint).c_str());
  this->send_request();
}

void download_client::exit(error_code e) {
  if (this->get_stage() != worker_completed) {
    this->set_stage_completed();
  } else {
    ERROR("Exit rejected. There can be only one exit");
    return;
  }
  this->callback(e);
}

void download_client::send_request() {
  XDEBUG("Sending request to download file %s", this->remote_file_name.c_str());
  this->frame = frame::create_read_request_frame(this->remote_file_name);
  this->sender.async_send(
      this->frame->get_asio_buffer(),
      this->remote_endpoint,
      [self = shared_from_this()](const boost::system::error_code &error, const std::size_t) {
        if (error) {
          ERROR("%s[%d] send failed Error :%s", __func__, __LINE__, to_string(error).c_str());
          self->exit(error::boost_asio_error_base + error.value());
        } else {
          self->receive_data();
        }
      });
}

void download_client::send_ack() { this->send_ack_for_block_number(this->block_number); }

void download_client::send_ack_for_block_number(uint16_t block_num) {
  XDEBUG("Sending ack for block number %u", block_num);
  this->frame = frame::create_ack_frame(block_num);
  this->sender.async_send(
      this->frame->get_asio_buffer(),
      this->server_tid,
      [self = shared_from_this()](const boost::system::error_code &error, const std::size_t) {
        if (error) {
          ERROR("%s[%d] Received unrecoverable error [%s]", __func__, __LINE__, to_string(error).c_str());
          self->exit(error::boost_asio_error_base + error.value());
          return;
        }
        if (self->is_last_block) {
          self->exit(error::no_error);
        } else {
          self->receive_data();
        }
      });
}

void download_client::receive_data() {
  XDEBUG("Waiting for block number %u", this->block_number + 1);
  this->frame = frame::create_empty_frame();
  this->receiver.async_receive(this->frame->get_asio_buffer(),
                               std::bind(&download_client::receive_data_cb,
                                         shared_from_this(),
                                         std::placeholders::_1,
                                         std::placeholders::_2));
}

void download_client::receive_data_cb(const boost::system::error_code &error,
                                      const std::size_t bytes_received) {
  if (this->get_stage() == worker_aborted) {
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
    this->server_tid = this->receiver.get_receive_endpoint();
  } else if (this->server_tid != this->receiver.get_receive_endpoint()) {
    WARN("Expecting data from %s but received from %s",
         to_string(this->server_tid).c_str(),
         to_string(this->receiver.get_receive_endpoint()).c_str());
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
    WARN("Failed to parse response from %s", to_string(this->receiver.get_receive_endpoint()).c_str());
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
  if (!this->write_handle.write_buffer(itr_pair.first, itr_pair.second)) {
    // Failed to write to file. Fatal error
    ERROR("IO Error on file %s. Aborting download", this->local_file_name.c_str());
    this->exit(error::disk_io_error);
    return;
  }
  this->retry_count = 0;
  if (itr_pair.second - itr_pair.first != TFTP_FRAME_MAX_DATA_LEN) {
    this->is_last_block = true;
  }
  this->send_ack();
}

//-----------------------------------------------------------------------------
/*
upload_client::upload_client(boost::asio::io_context &io,
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

void upload_client::sender(const boost::system::error_code &error, const std::size_t bytes_received) {
  // std::cout << this->remote_tid << " [" << __func__ << ":" << __LINE__ << "] "
  //          << " Stage :" << this->stage << std::endl;

  this->update_stage(error, bytes_received);
  switch (this->stage) {
  case upload_client::upload_request: {
    this->frame = frame::create_write_request_frame(this->file_name);
    this->socket.async_send_to(this->frame->get_asio_buffer(),
                               this->remote_tid,
                               std::bind(&upload_client::receiver,
                                         shared_from_this(),
                                         std::placeholders::_1,
                                         std::placeholders::_2));
    break;
  }
  case upload_client::upload_data: {
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
                               std::bind(&upload_client::receiver,
                                         shared_from_this(),
                                         std::placeholders::_1,
                                         std::placeholders::_2));
    break;
  }
  case upload_client::exit: {
    this->callback(this->exec_error);
    return;
  }
  default:
    std::cerr << this->remote_tid << " [" << __func__ << "] Stage :" << this->stage
              << " state machine reached invalid stage." << std::endl;
    break;
  }
}

void upload_client::receiver(const boost::system::error_code &error, const std::size_t bytes_sent) {
  // std::cout << this->remote_tid << " [" << __func__ << ":" << __LINE__ << "] "
  //          << " Stage :" << this->stage << std::endl;
  this->update_stage(error, bytes_sent);
  switch (this->stage) {
  case upload_client::wait_ack: {
    this->frame = frame::create_empty_frame();
    this->socket.async_receive_from(
        this->frame->get_asio_buffer(),
        this->remote_tid,
        std::bind(&upload_client::sender, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
    break;
  }
  case upload_client::exit: {
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

void upload_client::update_stage(const boost::system::error_code &error, const std::size_t bytes_transacted) {
  // TODO: Add necessary changes for errornous case
  (void)(error);
  (void)(bytes_transacted);
  switch (this->stage) {
  case upload_client::init: {
    this->stage = upload_client::upload_request;
    break;
  }
  case upload_client::upload_request: {
    this->stage = upload_client::wait_ack;
    break;
  }
  case upload_client::wait_ack: {
    this->block_number++;
    if (this->is_last_block) {
      this->stage = upload_client::exit;
    } else {
      this->stage = upload_client::upload_data;
    }
    break;
  }
  case upload_client::upload_data: {
    this->stage = upload_client::wait_ack;
    break;
  }
  case upload_client::exit:
  default: {
    break;
  }
  }
}*/

//-----------------------------------------------------------------------------
