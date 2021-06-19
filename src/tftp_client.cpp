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

//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------

base_client::base_client(boost::asio::io_context &io, const client_config &config)
    : server_endpoint(config.remote_endpoint),
      work_dir(config.work_dir),
      remote_file_name(config.remote_file_name),
      local_file_name(config.local_file_name),
      callback(config.callback),
      timeout(config.network_timeout),
      retry_count(config.retry_count),
      socket(io),
      timer(io),
      block_number(0),
      client_stage(client_constructed) { }

base_client::~base_client(){
  this->socket.close();
  //Better check if timer has anything waiting on it, Then only cancel
  this->timer.cancel();
  this->file_handle.close();
}

void base_client::exit(tftp::error_code e) {
  this->file_handle.flush();
  this->callback(e);
}

void base_client::start() {
  if(this->client_stage != client_constructed){
    return;
  }
  this->client_stage = client_running;
}


void base_client::stop() {
  this->socket.cancel();
  this->client_stage = client_aborted;
}

//-----------------------------------------------------------------------------

download_client::download_client(boost::asio::io_context &io, const download_client_config &config)
    : base_client(io, config), download_stage(dc_request_data), is_last_block(false), is_file_open(false)
{ }

download_client_s download_client::create(boost::asio::io_context &io, const download_client_config &config) {
  download_client_s self(new download_client(io, config));
  return self;
}

void download_client::start(){
  if(this->client_stage != client_constructed){
    base_client::start();
    this->download_stage = dc_request_data;
    this->sender();
  }
}

void download_client::stop() {
  base_client::stop();
  this->download_stage = dc_abort;
}

void download_client::sender() {
}

void download_client::sender_cb(const boost::system::error_code &error, const std::size_t bytes_sent) {
  (void)(error);
  (void)(bytes_sent);
}

void download_client::receiver() {
}

void download_client::receiver_cb(const boost::system::error_code &error, const std::size_t bytes_received){
  (void)(error);
  (void)(bytes_received);
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
  // std::cout << this->remote_tid << " [" << __func__ << ":" << __LINE__ << "] "
  //          << " Stage :" << this->stage << std::endl;

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
    try {
      this->frame->parse_frame();
    } catch (framing_exception &e) {
      // std::cout << this->remote_tid << " [" << __func__ << "] Failed to parse response from " << std::endl;
      this->callback(invalid_server_response);
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
      this->callback(invalid_server_response);
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
  // std::cout << this->remote_tid << " [" << __func__ << ":" << __LINE__ << "] "
  //          << " Stage :" << this->stage << std::endl;
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
