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

client_downloader::client_downloader(
    boost::asio::io_context &io, const std::string &file_name,
    const udp::endpoint &remote_endpoint,
    std::unique_ptr<std::ostream> u_out_stream,
    client_completion_callback download_callback)
    : io(io), socket(io), remote_tid(remote_endpoint), file_name(file_name),
      u_out(std::move(u_out_stream)), callback(download_callback) {
  socket.open(udp::v4());
  stage = init;
}

void client_downloader::sender(const boost::system::error_code &error,
                               const std::size_t bytes_received) {
  this->update_stage(error, bytes_received);
  switch (this->stage) {
  case client_downloader::request_data: {
    this->frame = tftp_frame::create_read_request_frame(this->file_name);
    this->socket.async_send_to(
        this->frame->get_asio_buffer(), this->remote_tid,
        std::bind(&client_downloader::receiver, shared_from_this(),
                  std::placeholders::_1, std::placeholders::_2));
    break;
  }
  case client_downloader::send_ack: {
    this->frame->resize(bytes_received);
    this->frame->parse_frame();
    auto itr_pair = this->frame->get_data_iterator();
    auto itr = itr_pair.first;
    auto itr_end = itr_pair.second;
    while (itr != itr_end) {
      std::cout << *itr << ",";
      (*this->u_out) << *itr;
      itr++;
    }
    this->frame = tftp_frame::create_ack_frame(this->frame->get_block_number());
    this->socket.async_send_to(
        this->frame->get_asio_buffer(), this->remote_tid,
        std::bind(&client_downloader::receiver, shared_from_this(),
                  std::placeholders::_1, std::placeholders::_2));
    break;
  }
  default:
    break;
  }
}

void client_downloader::receiver(const boost::system::error_code &error,
                                 const std::size_t bytes_sent) {
  this->update_stage(error, bytes_sent);
  switch (this->stage) {
  case client_downloader::receive_data: {
    this->frame = tftp_frame::create_empty_frame();
    this->socket.async_receive_from(
        this->frame->get_asio_buffer(), this->remote_tid,
        std::bind(&client_downloader::sender, shared_from_this(),
                  std::placeholders::_1, std::placeholders::_2));
    break;
  }
  case client_downloader::exit: {
    this->callback(this->exec_error);
  }
  default: {
    break;
  }
  }
}

void client_downloader::update_stage(const boost::system::error_code &error,
                                     const std::size_t bytes_transacted) {
  // TODO: Add necessary changes for errornous case
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
