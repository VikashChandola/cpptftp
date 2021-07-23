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
client::client(boost::asio::io_context &io, const client_config &config)
    : base_worker(io, config),
      remote_file_name(config.remote_file_name),
      local_file_name(config.local_file_name),
      callback(config.callback),
      is_last_block(false) {
  if (this->server_tid.port() != 0) {
    ERROR("server tids are not zero. client can not function");
    std::exit(1);
  }
}

void client::start() {
  if (this->get_stage() != worker_constructed) {
    NOTICE("start request rejected. Start can only be requested for freshly constructed objects.");
    return;
  }
  this->set_stage_running();
  this->send_request();
}

void client::exit(error_code e) {
  if (this->get_stage() != worker_completed) {
    this->set_stage_completed();
  } else {
    ERROR("Exit rejected. There can be only one exit");
    return;
  }
  this->callback(e);
}
//-----------------------------------------------------------------------------

download_client::download_client(boost::asio::io_context &io, const download_client_config &config)
    : client(io, config),
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
  DEBUG("Downloading file %s from server hosted at %s",
        this->local_file_name.c_str(),
        to_string(this->remote_endpoint).c_str());
  client::start();
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

upload_client::upload_client(boost::asio::io_context &io, const upload_client_config &config)
    : client(io, config),
      read_handle(config.local_file_name),
      receiver(io, this->socket, config.network_timeout),
      sender(io, this->socket, config.delay_gen) {}

upload_client_s upload_client::create(boost::asio::io_context &io, const upload_client_config &config) {
  upload_client_s self(new upload_client(io, config));
  return self;
}

void upload_client::start() {
  DEBUG("Uploading file %s to server hosted at %s",
        this->local_file_name.c_str(),
        to_string(this->remote_endpoint).c_str());
  client::start();
}

void upload_client::send_request() {
  XDEBUG("Sending request upload for file %s", this->remote_file_name.c_str());
  this->frame = frame::create_write_request_frame(this->remote_file_name);
  this->sender.async_send(
      this->frame->get_asio_buffer(),
      this->remote_endpoint,
      [self = shared_from_this()](const boost::system::error_code &error, const std::size_t) {
        if (error) {
          ERROR("%s[%d] send failed Error :%s", __func__, __LINE__, to_string(error).c_str());
          self->exit(error::boost_asio_error_base + error.value());
        } else {
          self->receive_ack();
        }
      });
}

void upload_client::receive_ack() {
  XDEBUG("Waiting ack for block number %u", this->block_number);
  this->frame = frame::create_empty_frame();
  this->receiver.async_receive(this->frame->get_asio_buffer(),
                               std::bind(&upload_client::receive_ack_cb,
                                         shared_from_this(),
                                         std::placeholders::_1,
                                         std::placeholders::_2));
}

void upload_client::receive_ack_cb(const boost::system::error_code &error, const std::size_t bytes_received) {
  if (this->get_stage() == worker_aborted) {
    this->exit(error::no_error);
    return;
  }
  if (error && error != boost::asio::error::operation_aborted) {
    std::cerr << this->remote_endpoint << " [" << __func__ << "] error :" << error << std::endl;
    return;
  }
  if (error == boost::asio::error::operation_aborted) {
    WARN("%s, Timed out while waiting while wait for response", to_string(this->remote_endpoint).c_str());
    if (this->retry_count++ >= this->max_retry_count) {
      this->exit(error::receive_timeout);
      return;
    }
    if (this->block_number == 0) {
      this->send_request();
    } else {
      this->send_data(true);
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
      this->receive_ack();
    }
    return;
  }
  try {
    this->frame->resize(bytes_received);
    this->frame->parse_frame(frame::op_ack);
  } catch (frame_type_mismatch_exception &e) {
    if (this->frame->get_op_code() == frame::op_error) {
      XDEBUG("Server responded with error :%s", this->frame->get_error_message().c_str());
      this->exit(this->frame->get_error_code());
      return;
    } else {
      this->receive_ack();
    }
  } catch (framing_exception &e) {
    std::cerr << this->remote_endpoint << " [" << __func__ << "] Failed to parse ack frame"
              << " Error :" << e.what() << std::endl;

    this->receive_ack();
    return;
  }
  if (this->frame->get_block_number() != this->block_number) {
    this->receive_ack();
    return;
  }
  this->retry_count = 0;
  if (this->is_last_block) {
    std::cout << this->remote_endpoint << " [" << __func__ << "] has been served." << std::endl;
    this->exit(error::no_error);
    return;
  }
  this->send_data();
}

void upload_client::send_data(bool resend) {
  // If it's resend request then don't update block number and data
  // Just send whatever we had sent last time
  if (!resend) {
    if (this->read_handle.read_buffer(&this->data[0],
                                      &this->data[TFTP_FRAME_MAX_DATA_LEN],
                                      this->data_size) == false) {
      // this->send_error(frame::undefined_error, "File read operation failed");
      // What to do here ?
      return;
    }
    is_last_block = (TFTP_FRAME_MAX_DATA_LEN != this->data_size);
    this->block_number++;
  } else {
    INFO("%s Resending block number", to_string(this->remote_endpoint).c_str());
  }
  this->frame =
      frame::create_data_frame(&this->data[0],
                               std::min(&this->data[this->data_size], &this->data[TFTP_FRAME_MAX_DATA_LEN]),
                               this->block_number);
  auto send_data_cb = [self = shared_from_this()](const boost::system::error_code &error,
                                                  const std::size_t &) {
    if (error) {
      std::cerr << self->remote_endpoint << "Failed to send data  error :" << error << std::endl;
    }
    self->receive_ack();
  };
  this->sender.async_send(this->frame->get_asio_buffer(), this->server_tid, send_data_cb);
}

//-----------------------------------------------------------------------------
