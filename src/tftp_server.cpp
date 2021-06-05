#include "tftp_server.hpp"
#include "tftp_frame.hpp"
#include <iostream>

using boost::asio::ip::udp;
using namespace tftp;

server::server(boost::asio::io_context &io, frame_csc &frame, const udp::endpoint &endpoint,
               const std::string &work_dir, const uint64_t &ms_timeout)
    : socket(io), client_endpoint(endpoint), filename(work_dir + "/" + frame->get_filename()), timer(io),
      timeout(boost::asio::chrono::milliseconds(ms_timeout)) {
  socket.open(udp::v4());
}

download_server::download_server(boost::asio::io_context &io, frame_csc &first_frame, const udp::endpoint &endpoint,
                                 const std::string &work_dir)
    : server(io, first_frame, endpoint, work_dir), stage(ds_send_data),
      read_stream(this->filename, std::ios::in | std::ios::binary), block_number(0), is_last_frame(false) {
  std::cout << this->client_endpoint << " Provisioning downloader_server object" << std::endl;
}

download_server::~download_server() {
  std::cout << this->client_endpoint << " Destorying downloader_server object for client" << std::endl;
}

void download_server::serve(boost::asio::io_context &io, frame_csc &frame, const udp::endpoint &endpoint,
                            const std::string &work_dir) {
  download_server_s self = std::make_shared<download_server>(io, frame, endpoint, work_dir);
  if (!self->read_stream.is_open()) {
    std::cerr << self->client_endpoint << " Failed to read '" << self->filename << "'" << std::endl;
    self->tftp_error_code = frame::file_not_found;
    self->stage = ds_send_error;
  }
  self->sender();
}

void download_server::sender() {
  std::cout << this->client_endpoint << " [" << __func__ << "] "
            << " Stage :" << this->stage << std::endl;
  switch (this->stage) {
  case download_server::ds_send_data: {
    if (this->fill_data_buffer() == false) {
      this->stage = ds_send_error;
      this->sender();
      return;
    }
    this->block_number++;
    this->frame = frame::create_data_frame(&this->data[0],
                                           std::min(&this->data[this->data_size], &this->data[TFTP_FRAME_MAX_DATA_LEN]),
                                           this->block_number);
    this->socket.async_send_to(
        this->frame->get_asio_buffer(), this->client_endpoint,
        std::bind(&download_server::sender_cb, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
    return;
  } break;
  case download_server::ds_resend_data: {
    this->frame = frame::create_data_frame(&this->data[0],
                                           std::min(&this->data[this->data_size], &this->data[TFTP_FRAME_MAX_DATA_LEN]),
                                           this->block_number);
    this->socket.async_send_to(
        this->frame->get_asio_buffer(), this->client_endpoint,
        std::bind(&download_server::sender_cb, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
    return;
  } break;
  case ds_send_error: {
    this->frame = frame::create_error_frame(this->tftp_error_code);
    this->socket.async_send_to(
        this->frame->get_asio_buffer(), this->client_endpoint,
        std::bind(&download_server::sender_cb, shared_from_this(), std::placeholders::_1, std::placeholders::_2));

  } break;
  default: {
    std::cerr << this->client_endpoint << " [" << __func__ << "] Stage :" << this->stage
              << " state machine reached invalid stage." << std::endl;
  } break;
  }
}

bool download_server::fill_data_buffer() {
  if (!this->read_stream.is_open()) {
    return false;
  }
  this->read_stream.read(this->data, TFTP_FRAME_MAX_DATA_LEN);
  this->data_size = this->read_stream.gcount();
  if (this->read_stream.eof() || this->data_size < TFTP_FRAME_MAX_DATA_LEN) {
    this->is_last_frame = true;
  }
  return true;
}

void download_server::sender_cb(const boost::system::error_code &error, const std::size_t &bytes_sent) {
  (void)(bytes_sent);
  std::cout << this->client_endpoint << " [" << __func__ << "] Stage :" << this->stage << std::endl;
  if (error) {
    std::cout << this->client_endpoint << " [" << __func__ << "] error :" << error << std::endl;
    return;
  }
  switch (this->stage) {
  case ds_send_data:
  case ds_resend_data: {
    this->stage = ds_recv_ack;
    this->receiver();
  } break;
  case ds_send_error: {
  } break;
  default: {
    std::cerr << this->client_endpoint << " [" << __func__ << "] Stage :" << this->stage
              << " state machine reached invalid stage." << std::endl;
  } break;
  }
}

void download_server::receiver() {
  std::cout << this->client_endpoint << " [" << __func__ << "] Stage :" << this->stage << std::endl;
  switch (this->stage) {
  case download_server::ds_recv_ack: {
    this->frame = frame::create_empty_frame();
    this->socket.async_receive_from(
        this->frame->get_asio_buffer(), this->receive_endpoint,
        std::bind(&download_server::receiver_cb, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
    this->timer.expires_after(this->timeout);
    this->timer.async_wait([&](const boost::system::error_code &error) {
      if (error == boost::asio::error::operation_aborted) {
        return;
      }
      this->stage = ds_recv_timeout;
      std::cout << this->client_endpoint << " timed out on receive." << std::endl;
      this->socket.cancel();
    });
    // Add timer related stuff here
  } break;
  default: {
  } break;
  }
}

void download_server::receiver_cb(const boost::system::error_code &error, const std::size_t &bytes_received) {
  if (error && error != boost::asio::error::operation_aborted) {
    std::cout << this->client_endpoint << " [" << __func__ << "] error :" << error << std::endl;
    return;
  }
  this->timer.cancel();
  switch (this->stage) {
  case ds_recv_ack: {
    this->retry_count = 0;
    if (this->receive_endpoint != this->client_endpoint) {
      // somebody is fucking up on this udp port
      std::cout << this->client_endpoint << " [" << __func__ << "] Received response from wrong endpoint ["
                << this->receive_endpoint << "]" << std::endl;
      this->receiver();
      return;
    }
    try {
      this->frame->resize(bytes_received);
      this->frame->parse_frame();
    } catch (framing_exception &e) {
      // bad frame, check again
      std::cout << this->client_endpoint << " [" << __func__ << "] Failed to parse ack frame"
                << " Error :" << e.what() << std::endl;
      this->receiver();
      return;
    }
    if (this->frame->get_block_number() != this->block_number) {
      // received ack for different block number, discard it
      this->receiver();
      return;
    }
    if (this->is_last_frame) {
      std::cout << this->client_endpoint << " [" << __func__ << "] has been served." << std::endl;
      // congratualtions, 'this' have served it's purpose, time to die now
      return;
    }
    // Everything is good, time to send new data frame to client
    this->stage = ds_send_data;
    this->sender();
    return;
  } break;
  case ds_recv_timeout: {
    std::cout << this->client_endpoint << " [" << __func__ << "] Resending block number :" << this->block_number
              << std::endl;
    // no response from client, may be packet got lost. Retry
    if (this->retry_count++ != download_server::max_retry_count) {
      this->stage = ds_resend_data;
      this->sender();
      return;
    } else {
      // ok, enough retries, client is dead. Let's jump of the cliff
      std::cerr << this->client_endpoint << " [" << __func__ << "] Failed to receive ack " << std::endl;
      return;
    }
    return;
  } break;
  default: {
    std::cerr << this->client_endpoint << " [" << __func__ << "] Stage :" << this->stage
              << " state machine reached invalid stage." << std::endl;
    return;
  } break;
  }
}

void spin_tftp_server(boost::asio::io_context &io, frame_csc &first_frame, const udp::endpoint &client_endpoint,
                      const std::string &work_dir) {
  switch (first_frame->get_op_code()) {
  case frame::op_read_request:
    download_server::serve(io, first_frame, client_endpoint, work_dir);
    break;
  case frame::op_write_request:
    break;
  default:
    break;
  }
}

distributor::distributor(boost::asio::io_context &io, const udp::endpoint &local_endpoint, std::string &work_dir)
    : io(io), socket(io, local_endpoint), work_dir(work_dir), server_count(0) {}

distributor_s distributor::create(boost::asio::io_context &io, const udp::endpoint &local_endpoint,
                                  std::string work_dir) {
  return std::make_shared<distributor>(distributor(io, local_endpoint, work_dir));
}
/*
distributor_s distributor::create(boost::asio::io_context &io, const uint16_t udp_port, std::string work_dir){
}*/

uint64_t distributor::start_service() {
  std::cout << "Starting distribution on :" << this->socket.local_endpoint() << std::endl;
  this->perform_distribution();
  return server_count;
}

uint64_t distributor::stop_service() {
  std::cout << "Stopping distribution on :" << this->socket.local_endpoint() << std::endl;
  this->socket.cancel();
  return server_count;
}

void distributor::perform_distribution() {
  this->first_frame = frame::create_empty_frame();
  this->socket.async_receive_from(this->first_frame->get_asio_buffer(), this->remote_endpoint,
                                  std::bind(&distributor::perform_distribution_cb, shared_from_this(),
                                            std::placeholders::_1, std::placeholders::_2));
}

void distributor::perform_distribution_cb(const boost::system::error_code &error, const std::size_t &bytes_received) {
  if (error == boost::asio::error::operation_aborted) {
    return;
  }
  this->first_frame->resize(bytes_received);
  this->first_frame->parse_frame();
  std::cout << "Received request from " << this->remote_endpoint << std::endl;
  spin_tftp_server(this->io, this->first_frame, this->remote_endpoint, this->work_dir);
  this->perform_distribution();
}
