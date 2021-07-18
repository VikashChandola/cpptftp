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
  this->sender.async_send(this->frame->get_asio_buffer(), this->remote_endpoint, send_data_cb);
}

void download_server::receive_ack() {
  this->frame = frame::create_empty_frame();
  this->receiver.async_receive(this->frame->get_asio_buffer(),
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
    this->frame->resize(bytes_received);
    this->frame->parse_frame();
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
  if (this->is_last_frame) {
    std::cout << this->remote_endpoint << " [" << __func__ << "] has been served." << std::endl;
    this->exit(error::no_error);
    return;
  }
  this->send_data();
}

void download_server::send_error(const frame::error_code &e_code, const std::string &message) {
  this->frame = frame::create_error_frame(e_code, message);
  this->sender.async_send(
      this->frame->get_asio_buffer(),
      this->remote_endpoint,
      [self = shared_from_this()](const boost::system::error_code &, const std::size_t &) {
        self->exit(error::disk_io_error);
      });
}

//-----------------------------------------------------------------------------
/*
upload_server::upload_server(boost::asio::io_context &io, const upload_server_config &config)
    : server(io, config),
      stage(us_send_ack) {
  std::cout << this->remote_endpoint << " Provisioned upload server job" << std::endl;
}

upload_server::~upload_server() {
  std::cout << this->remote_endpoint << " Destroyed upload server job" << std::endl;
}

upload_server_s upload_server::create(boost::asio::io_context &io, const upload_server_config &config) {
  upload_server_s self(new upload_server(io, config));
  return self;
}

void upload_server::start() {
  if (std::filesystem::exists(this->filename)) {
    std::cerr << this->remote_endpoint << ": '" << this->filename << "' File already exists." << std::endl;
    this->tftp_error_code = frame::file_already_exists;
    this->stage           = us_send_error;
  } else {
    this->write_stream = std::ofstream(this->filename, std::ios::out | std::ios::binary);
    if (!this->write_stream.is_open()) {
      std::cerr << this->remote_endpoint << " Failed to open '" << this->filename << "'" << std::endl;
      this->tftp_error_code = frame::file_not_found;
      this->stage           = us_send_error;
    }
  }
  this->sender();
};

void upload_server::sender() {
  // std::cout << this->remote_endpoint << " [" << __func__ << "] Stage :" << this->stage << std::endl;
  switch (this->stage) {
  case us_send_ack: {
    this->frame = frame::create_ack_frame(this->block_number);
    this->socket.async_send_to(this->frame->get_asio_buffer(),
                               this->remote_endpoint,
                               std::bind(&upload_server::sender_cb,
                                         shared_from_this(),
                                         std::placeholders::_1,
                                         std::placeholders::_2));
    return;
  } break;
  case us_send_error: {
    this->frame = frame::create_error_frame(this->tftp_error_code);
    this->socket.async_send_to(this->frame->get_asio_buffer(),
                               this->remote_endpoint,
                               std::bind(&upload_server::sender_cb,
                                         shared_from_this(),
                                         std::placeholders::_1,
                                         std::placeholders::_2));
    return;
  } break;
  default: {
    std::cerr << this->remote_endpoint << " [" << __func__ << "] Stage :" << this->stage
              << " state machine reached invalid stage." << std::endl;
  } break;
  }
}

void upload_server::sender_cb(const boost::system::error_code &error, const std::size_t &bytes_sent) {
  (void)(bytes_sent);
  // std::cout << this->remote_endpoint << " [" << __func__ << "] Stage :" << this->stage << std::endl;
  if (error) {
    std::cerr << this->remote_endpoint << " [" << __func__ << "] error :" << error << std::endl;
    return;
  }
  switch (this->stage) {
  case us_resend_ack:
  case us_send_ack: {
    if (this->is_last_frame) {
      std::cout << this->remote_endpoint << " [" << __func__ << "] has been served." << std::endl;
      return;
    }
    this->stage = us_recv_data;
    this->receiver();
    return;
  } break;
  case us_send_error: {
    return;
  } break;
  default: {
    std::cerr << this->remote_endpoint << " [" << __func__ << "] Stage :" << this->stage
              << " state machine reached invalid stage." << std::endl;
  } break;
  }
}

void upload_server::receiver() {
  // std::cout << this->remote_endpoint << " [" << __func__ << "] Stage :" << this->stage << std::endl;
  switch (this->stage) {
  case us_recv_data: {
    this->frame = frame::create_empty_frame();
    this->socket.async_receive_from(this->frame->get_asio_buffer(),
                                    this->receive_endpoint,
                                    std::bind(&upload_server::receiver_cb,
                                              shared_from_this(),
                                              std::placeholders::_1,
                                              std::placeholders::_2));
    this->timer.expires_after(this->network_timeout);
    this->timer.async_wait([&](const boost::system::error_code &error) {
      if (error == boost::asio::error::operation_aborted) {
        return;
      }
      this->stage = us_recv_timeout;
      std::cout << this->remote_endpoint << " timed out on receive." << std::endl;
      this->socket.cancel();
    });
    return;
  } break;
  default: {
    std::cerr << this->remote_endpoint << " [" << __func__ << "] Stage :" << this->stage
              << " state machine reached invalid stage." << std::endl;
  } break;
  }
}

void upload_server::receiver_cb(const boost::system::error_code &error, const std::size_t &bytes_received) {
  // std::cout << this->remote_endpoint << " [" << __func__ << "] Stage :" << this->stage << std::endl;
  if (error && error != boost::asio::error::operation_aborted) {
    std::cout << this->remote_endpoint << " [" << __func__ << "] error :" << error << std::endl;
    return;
  }
  this->timer.cancel();
  switch (this->stage) {
  case us_recv_data: {
    if (this->receive_endpoint != this->remote_endpoint) {
      this->retry_count++;
      std::cerr << this->remote_endpoint << " [" << __func__ << "] Received response from wrong endpoint ["
                << this->receive_endpoint << "]" << std::endl;
      this->receiver();
      return;
    }
    try {
      this->frame->resize(bytes_received);
      this->frame->parse_frame();
    } catch (framing_exception &e) {
      std::cerr << this->remote_endpoint << " [" << __func__ << "] Failed to parse data frame"
                << " Error :" << e.what() << std::endl;
      this->retry_count++;
      this->receiver();
      return;
    }
    if (this->frame->get_op_code() != frame::op_data) {
      std::cerr << this->remote_endpoint << " [" << __func__ << "] Invalid tftp frame received " << std::endl;
      this->retry_count++;
      this->receiver();
      return;
    }
    this->retry_count  = 0;
    this->block_number = this->frame->get_block_number();
    auto data          = this->frame->get_data_iterator();
    std::for_each(data.first, data.second, [&](const char &ch) { this->write_stream << ch; });
    if (data.second - data.first != TFTP_FRAME_MAX_DATA_LEN) {
      this->is_last_frame = true;
    }
    this->stage = us_send_ack;
    this->sender();
    return;
  } break;
  case us_recv_timeout: {
    std::cout << this->remote_endpoint << " [" << __func__
              << "] Resending ack for block number :" << this->block_number << std::endl;
    if (this->retry_count++ != upload_server::max_retry_count) {
      this->stage = us_recv_data;
      this->receiver();
      return;
    } else {
      std::cerr << this->remote_endpoint << " [" << __func__ << "] Failed to receive any data from client"
                << std::endl;
      return;
    }
    return;
  } break;
  default: {
  } break;
  }
}*/

void spin_tftp_server(boost::asio::io_context &io,
                      frame_csc &first_frame,
                      const udp::endpoint &remote_endpoint,
                      const std::string &work_dir) {
  switch (first_frame->get_op_code()) {
  case frame::op_read_request: {
    download_server_config config(remote_endpoint, work_dir, first_frame);
    auto ds = download_server::create(io, config);
    ds->start();
  } break;
  case frame::op_write_request: {
    upload_server_config config(remote_endpoint, work_dir, first_frame);
    /*auto us = upload_server::create(io, config);
    us->start();*/
    break;
  }
  default:
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
  return std::make_shared<server_distributor>(server_distributor(io, local_endpoint, work_dir));
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
  this->first_frame = frame::create_empty_frame();
  this->socket.async_receive_from(this->first_frame->get_asio_buffer(),
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
  this->first_frame->resize(bytes_received);
  try {
    this->first_frame->parse_frame();
  } catch (framing_exception &e) {
    std::cout << "Received bad data from " << this->remote_endpoint << std::endl;
    this->perform_distribution();
    return;
  }
  std::cout << "Received request from " << this->remote_endpoint << std::endl;
  spin_tftp_server(this->io, this->first_frame, this->remote_endpoint, this->work_dir);
  this->perform_distribution();
}
