#include "tftp_server.hpp"
#include "tftp_frame.hpp"
#include <filesystem>
#include <iostream>

#include "log.hpp"

using boost::asio::ip::udp;
using namespace tftp;

server::server(boost::asio::io_context &io, const server_config &config)
    : base_worker(io, config),
      filename(config.filename),
      is_last_frame(false) {}

download_server::download_server(boost::asio::io_context &io, const download_server_config &config)
    : server(io, config),
      read_handle(this->filename),
      receiver(io, this->socket, config.network_timeout),
      sender(io, this->socket, config.delay_gen) {
  std::cout << this->remote_endpoint << " Provisioned download_server object" << std::endl;
}

download_server::~download_server() {
  std::cout << this->remote_endpoint << " Destroyed download_server object" << std::endl;
}

download_server_s download_server::create(boost::asio::io_context &io, const download_server_config &config) {
  download_server_s self(new download_server(io, config));
  return self;
}

void download_server::start() {
  if (this->get_stage() != worker_constructed) {
    return;
  }
  this->set_stage_running();
  if (!this->read_handle.is_open()) {
    ERROR("[%s] Failed to open %s", to_string(this->remote_endpoint).c_str(), this->filename.c_str());
    if (std::filesystem::exists(this->filename)) {
      this->send_error(frame::access_violation);
    } else {
      this->send_error(frame::file_not_found);
    }
    return;
  }
  this->send_data();
};

void download_server::send_data(const bool &resend) {
  // If it's resend request then don't update block number and data
  // Just send whatever we had sent last time
  if (!resend) {
    if (this->read_handle.read_buffer(&this->data[0],
                                      &this->data[TFTP_FRAME_MAX_DATA_LEN],
                                      this->data_size) == false) {
      this->send_error(frame::undefined_error, "File read operation failed");
      return;
    }
    is_last_frame = (TFTP_FRAME_MAX_DATA_LEN != this->data_size);
    this->block_number++;
  } else {
    INFO("%s Resending block number", to_string(this->remote_endpoint).c_str());
  }
  this->network_frame.reset();
  this->network_frame.make_data_frame(
      &this->data[0],
      std::min(&this->data[this->data_size], &this->data[TFTP_FRAME_MAX_DATA_LEN]),
      this->block_number);
  auto send_data_cb = [self = shared_from_this()](const boost::system::error_code &error,
                                                  const std::size_t &) {
    if (error) {
      std::cerr << self->remote_endpoint << "Failed to send data  error :" << error << std::endl;
    }
    self->receive_ack();
  };
  this->sender.async_send(this->network_frame.get_asio_buffer(), this->remote_endpoint, send_data_cb);
}

void download_server::receive_ack() {
  this->network_frame.reset();
  this->receiver.async_receive(this->network_frame.get_asio_buffer(),
                               std::bind(&download_server::receive_ack_cb,
                                         shared_from_this(),
                                         std::placeholders::_1,
                                         std::placeholders::_2));
}

void download_server::receive_ack_cb(const boost::system::error_code &error,
                                     const std::size_t &bytes_received) {
  if (error && error != boost::asio::error::operation_aborted) {
    std::cerr << this->remote_endpoint << " [" << __func__ << "] error :" << error << std::endl;
    return;
  }
  if (this->get_stage() == worker_aborted) {
    this->exit(error::no_error);
    return;
  }
  if (error == boost::asio::error::operation_aborted) {
    WARN("%s, Timed out while waiting while wait for response", to_string(this->remote_endpoint).c_str());
    if (this->retry_count++ >= this->max_retry_count) {
      this->exit(error::receive_timeout);
      return;
    }
    this->send_data(true);
    return;
  }
  if (this->receiver.get_receive_endpoint() != this->remote_endpoint) {
    WARN("%s Received data from unknown endpoint %s. Message is Rejected",
         to_string(this->remote_endpoint).c_str(),
         to_string(this->receiver.get_receive_endpoint()).c_str());
    if (this->retry_count++ >= this->max_retry_count) {
      this->exit(error::network_interference);
      return;
    }
    this->receive_ack();
    return;
  }
  try {
    this->network_frame.resize(bytes_received);
    this->network_frame.parse_frame(frame::op_ack);
  } catch (framing_exception &e) {
    std::cerr << this->remote_endpoint << " [" << __func__ << "] Failed to parse ack frame"
              << " Error :" << e.what() << std::endl;
    this->receive_ack();
    return;
  }
  if (this->network_frame.get_block_number() != this->block_number) {
    this->receive_ack();
    return;
  }
  this->retry_count = 0;
  if (this->is_last_frame) {
    std::cout << this->remote_endpoint << " [" << __func__ << "] has been served." << std::endl;
    this->exit(error::no_error);
    return;
  }
  this->send_data();
}

void download_server::send_error(const frame::error_code &e_code, const std::string &message) {
  this->network_frame.reset();
  this->network_frame.make_error_frame(e_code, message);
  this->sender.async_send(
      this->network_frame.get_asio_buffer(),
      this->remote_endpoint,
      [self = shared_from_this()](const boost::system::error_code &, const std::size_t &) {
        self->exit(error::disk_io_error);
      });
}

//-----------------------------------------------------------------------------
upload_server::upload_server(boost::asio::io_context &io, const upload_server_config &config)
    : server(io, config),
      write_handle(config.filename),
      receiver(io, this->socket, config.network_timeout),
      sender(io, this->socket, config.delay_gen) {
  DEBUG("Serving upload request for file [%s] from client at [%s]",
        config.filename.c_str(),
        to_string(this->remote_endpoint).c_str());
}

upload_server::~upload_server() { DEBUG("Served client %s", to_string(this->remote_endpoint).c_str()); }

upload_server_s upload_server::create(boost::asio::io_context &io, const upload_server_config &config) {
  upload_server_s self(new upload_server(io, config));
  return self;
}

void upload_server::start() {
  if (this->get_stage() != worker_constructed) {
    return;
  }
  this->set_stage_running();
  if (std::filesystem::exists(this->filename)) {
    this->send_error(frame::file_already_exists);
  } else {
    this->send_ack(0);
  }
}

void upload_server::send_error(const frame::error_code &e_code, const std::string &message) {
  this->network_frame.reset();
  this->network_frame.make_error_frame(e_code, message);
  this->sender.async_send(
      this->network_frame.get_asio_buffer(),
      this->remote_endpoint,
      [self = shared_from_this()](const boost::system::error_code &, const std::size_t &) {
        self->exit(error::disk_io_error);
      });
}

void upload_server::send_ack(const uint16_t &block_num) {
  XDEBUG("Sending ack for block number %u", block_num);
  this->network_frame.reset();
  this->network_frame.make_ack_frame(block_num);
  this->sender.async_send(
      this->network_frame.get_asio_buffer(),
      this->remote_endpoint,
      [self = shared_from_this()](const boost::system::error_code &error, const std::size_t) {
        if (error) {
          ERROR("%s[%d] Received unrecoverable error [%s]", __func__, __LINE__, to_string(error).c_str());
          self->exit(error::boost_asio_error_base + error.value());
          return;
        }
        if (self->is_last_frame) {
          self->exit(error::no_error);
        } else {
          self->receive_data();
        }
      });
}

void upload_server::receive_data() {
  XDEBUG("Waiting for data from :%s", to_string(this->remote_endpoint).c_str());
  this->network_frame.reset();
  this->receiver.async_receive(this->network_frame.get_asio_buffer(),
                               std::bind(&upload_server::receive_data_cb,
                                         shared_from_this(),
                                         std::placeholders::_1,
                                         std::placeholders::_2));
}

void upload_server::receive_data_cb(const boost::system::error_code &error,
                                    const std::size_t &bytes_received) {
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
      this->send_ack(this->block_number);
    }
    return;
  }
  if (this->remote_endpoint != this->receiver.get_receive_endpoint()) {
    WARN("Expecting data from %s but received from %s",
         to_string(this->remote_endpoint).c_str(),
         to_string(this->receiver.get_receive_endpoint()).c_str());
    if (this->retry_count++ >= this->max_retry_count) {
      this->exit(error::invalid_server_response);
      return;
    } else {
      this->receive_data();
    }
    return;
  }
  try {
    this->network_frame.resize(bytes_received);
    this->network_frame.parse_frame(frame::op_data);
  } catch (framing_exception &e) {
    WARN("Failed to parse response from %s", to_string(this->receiver.get_receive_endpoint()).c_str());
    this->receive_data();
    return;
  }
  XDEBUG("Received block number %u", this->network_frame.get_block_number());
  if (this->block_number + 1 != this->network_frame.get_block_number()) {
    WARN("Expected blocks number %u got %u", this->block_number + 1, this->network_frame.get_block_number());
    WARN("Block %u is rejected", this->network_frame.get_block_number());
    this->send_ack(this->block_number);
    return;
  }
  this->block_number = this->network_frame.get_block_number();
  if (!this->write_handle.write_buffer(this->network_frame.data_cbegin(),
                                       this->network_frame.data_cbegin())) {
    // Failed to write to file. Fatal error
    ERROR("IO Error on file %s. Aborting Upload", this->filename.c_str());
    this->exit(error::disk_io_error);
    return;
  }
  XDEBUG("Received block number %d", this->block_number);
  this->retry_count = 0;
  if (this->network_frame.data_cend() - this->network_frame.data_cbegin() != TFTP_FRAME_MAX_DATA_LEN) {
    this->is_last_frame = true;
  }
  this->send_ack(this->block_number);
}
//-----------------------------------------------------------------------------

void spin_tftp_server(boost::asio::io_context &io,
                      const frame &first_frame,
                      const udp::endpoint &remote_endpoint,
                      const std::string &work_dir) {
  switch (first_frame.get_op_code()) {
  case frame::op_read_request: {
    download_server_config config(remote_endpoint, work_dir, first_frame);
    auto ds = download_server::create(io, config);
    ds->start();
  } break;
  case frame::op_write_request: {
    upload_server_config config(remote_endpoint, work_dir, first_frame);
    auto us = upload_server::create(io, config);
    us->start();
    break;
  }
  default:
    ERROR("No worker to handle %s", to_string(remote_endpoint).c_str());
    break;
  }
}

server_distributor::server_distributor(boost::asio::io_context &io,
                                       const udp::endpoint &local_endpoint,
                                       std::string &work_dir)
    : io(io),
      socket(io, local_endpoint),
      work_dir(work_dir),
      server_count(0) {}

server_distributor_s server_distributor::create(boost::asio::io_context &io,
                                                const udp::endpoint &local_endpoint,
                                                std::string work_dir) {
  return server_distributor_s(new server_distributor(io, local_endpoint, work_dir));
}

uint64_t server_distributor::start_service() {
  std::cout << "Starting distribution on " << this->socket.local_endpoint() << std::endl;
  this->perform_distribution();
  return server_count;
}

uint64_t server_distributor::stop_service() {
  std::cout << "Stopping distribution on :" << this->socket.local_endpoint() << std::endl;
  this->socket.cancel();
  return server_count;
}

void server_distributor::perform_distribution() {
  this->first_frame.reset();
  this->socket.async_receive_from(this->first_frame.get_asio_buffer(),
                                  this->remote_endpoint,
                                  std::bind(&server_distributor::perform_distribution_cb,
                                            shared_from_this(),
                                            std::placeholders::_1,
                                            std::placeholders::_2));
}

void server_distributor::perform_distribution_cb(const boost::system::error_code &error,
                                                 const std::size_t &bytes_received) {
  if (error == boost::asio::error::operation_aborted) {
    return;
  }
  this->first_frame.resize(bytes_received);
  try {
    this->first_frame.parse_frame();
  } catch (framing_exception &e) {
    std::cout << "Received bad data from " << this->remote_endpoint << std::endl;
    this->perform_distribution();
    return;
  }
  std::cout << "Received request from " << this->remote_endpoint << std::endl;
  spin_tftp_server(this->io, this->first_frame, this->remote_endpoint, this->work_dir);
  this->perform_distribution();
}
