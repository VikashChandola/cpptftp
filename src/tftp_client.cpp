#include <array>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>

#include <boost/asio.hpp>

#include "tftp_client.hpp"
#include "tftp_error_code.hpp"
#include "tftp_exception.hpp"

using boost::asio::ip::udp;
using namespace tftp;

client_downloader::client_downloader(boost::asio::io_context &io, const std::string &file_name,
                                     const udp::endpoint &remote_endpoint, std::unique_ptr<std::ostream> u_out_stream,
                                     client_completion_callback download_callback)
    : socket(io), remote_tid(remote_endpoint), file_name(file_name), u_out(std::move(u_out_stream)),
      callback(download_callback), exec_error(0), timer(io), timeout(boost::asio::chrono::seconds(1)) {
  socket.open(udp::v4());
  stage = init;
}

client_downloader_s client_downloader::create(boost::asio::io_context &io, const std::string &file_name,
                                              const udp::endpoint &remote_endpoint,
                                              std::unique_ptr<std::ostream> u_out_stream,
                                              client_completion_callback download_callback) {
  client_downloader_s self(
      new client_downloader(io, file_name, remote_endpoint, std::move(u_out_stream), download_callback));
  self->sender(boost::system::error_code(), 0);
  return self;
}

void client_downloader::sender(const boost::system::error_code &error, const std::size_t bytes_received) {
  if (error == boost::asio::error::operation_aborted) {
    return;
  }
  this->timer.cancel();
  this->update_stage(error, bytes_received);
  switch (this->stage) {
  case client_downloader::request_data: {
    this->frame = frame::create_read_request_frame(this->file_name);
    this->socket.async_send_to(
        this->frame->get_asio_buffer(), this->remote_tid,
        std::bind(&client_downloader::receiver, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
    break;
  }
  case client_downloader::send_ack: {
    this->frame->resize(bytes_received);
    this->frame->parse_frame();
    switch (this->frame->get_op_code()) {
    case frame::op_error:
      std::cerr << "Server responded with error :" << this->frame->get_error_message() << std::endl;
      this->callback(server_error_response);
      return;
      break;
    case frame::op_data:
      break;
    default:
      std::cerr << "Expected data frame received received frame with op code :" << this->frame->get_op_code()
                << std::endl;
      this->callback(invalid_server_response);
      return;
      break;
    }
    auto itr_pair = this->frame->get_data_iterator();
    auto itr = itr_pair.first;
    auto itr_end = itr_pair.second;
    while (itr != itr_end) {
      (*this->u_out) << *itr;
      itr++;
    }
    this->frame = frame::create_ack_frame(this->frame->get_block_number());
    this->socket.async_send_to(
        this->frame->get_asio_buffer(), this->remote_tid,
        std::bind(&client_downloader::receiver, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
    break;
  }
  default:
    break;
  }
}

void client_downloader::receiver(const boost::system::error_code &error, const std::size_t bytes_sent) {
  if (error == boost::asio::error::operation_aborted) {
    return;
  }
  this->update_stage(error, bytes_sent);
  switch (this->stage) {
  case client_downloader::receive_data: {
    this->frame = frame::create_empty_frame();
    this->socket.async_receive_from(
        this->frame->get_asio_buffer(), this->remote_tid,
        std::bind(&client_downloader::sender, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
    this->timer.expires_after(this->timeout);
    this->timer.async_wait([&](const boost::system::error_code error) {
      if (error == boost::asio::error::operation_aborted) {
        return;
      }
      this->socket.cancel();
      this->callback(receive_timeout);
    });
    break;
  }
  case client_downloader::exit: {
    this->u_out->flush();
    this->callback(this->exec_error);
  }
  default: {
    break;
  }
  }
}

void client_downloader::update_stage(const boost::system::error_code &error, const std::size_t bytes_transacted) {
  // TODO: Add necessary changes for errornous case
  (void)(error);
  switch (this->stage) {
  case client_downloader::init: {
    this->stage = client_downloader::request_data;
    break;
  }
  case client_downloader::request_data: {
    this->stage = client_downloader::receive_data;
    break;
  }
  case client_downloader::receive_data: {
    if (bytes_transacted != 516) {
      is_last_block = true;
    }
    this->stage = client_downloader::send_ack;
    break;
  }
  case client_downloader::send_ack: {
    if (is_last_block) {
      this->stage = client_downloader::exit;
    } else {
      this->stage = client_downloader::receive_data;
    }
    break;
  }
  case client_downloader::exit:
  default: {
    break;
  }
  }
}

//-----------------------------------------------------------------------------

client_uploader::client_uploader(boost::asio::io_context &io, const std::string &file_name,
                                 const udp::endpoint &remote_endpoint, std::unique_ptr<std::istream> u_in_stream,
                                 client_completion_callback upload_callback)
    : socket(io), remote_tid(remote_endpoint), file_name(file_name), u_in(std::move(u_in_stream)),
      callback(upload_callback), exec_error(0), block_number(0) {
  socket.open(udp::v4());
  stage = init;
}

void client_uploader::sender(const boost::system::error_code &error, const std::size_t bytes_received) {
  std::cout << this->remote_tid << " [" << __func__ << ":" << __LINE__ << "] "
            << " Stage :" << this->stage << std::endl;

  this->update_stage(error, bytes_received);
  switch (this->stage) {
  case client_uploader::upload_request: {
    this->frame = frame::create_write_request_frame(this->file_name);
    this->socket.async_send_to(
        this->frame->get_asio_buffer(), this->remote_tid,
        std::bind(&client_uploader::receiver, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
    break;
  }
  case client_uploader::upload_data: {
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
    this->socket.async_send_to(
        this->frame->get_asio_buffer(), this->remote_tid,
        std::bind(&client_uploader::receiver, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
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
  std::cout << this->remote_tid << " [" << __func__ << "] "
            << " Stage :" << this->stage << std::endl;
  this->update_stage(error, bytes_sent);
  switch (this->stage) {
  case client_uploader::wait_ack: {
    this->frame = frame::create_empty_frame();
    this->socket.async_receive_from(
        this->frame->get_asio_buffer(), this->remote_tid,
        std::bind(&client_uploader::sender, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
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

void client_uploader::update_stage(const boost::system::error_code &error, const std::size_t bytes_transacted) {
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
