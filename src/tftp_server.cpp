#include <iostream>
#include "tftp_server.hpp"
#include "tftp_frame.hpp"

using boost::asio::ip::udp;
using namespace tftp;

server::server(boost::asio::io_context &io, frame_csc &frame, const udp::endpoint &endpoint)
    : socket(io), client_endpoint(endpoint), filename(frame->get_filename()) {
}

download_server::download_server(boost::asio::io_context &io, frame_csc &first_frame, 
                                 const udp::endpoint &endpoint)
    : server(io, first_frame, endpoint), stage(ds_init),
      read_stream(this->filename, std::ios::in | std::ios::binary),
      block_number(0), is_last_frame(false) {
  //std::fstream ff(this->filename, std::ios::in | std::ios::binary);
  //this->istream = std::move(ff);
}

void download_server::serve(boost::asio::io_context &io, frame_csc &frame, const udp::endpoint &endpoint) {
  std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "]" << std::endl;
  download_server_u self = std::make_unique<download_server>(io, frame, endpoint);
  bool is_open = self->read_stream.is_open();
  uint16_t port = self->client_endpoint.port();
  std::cout << "Sending to :" << port << std::endl;
  download_server::sender(self, boost::system::error_code(), 0);
}

void download_server::sender(download_server_u &self, const boost::system::error_code &e,
                             std::size_t bytes_received) {
  (void)(e);
  std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "]" << std::endl;
  self->update_stage(bytes_received);
  switch(self->stage) {
  case ds_send_data:
  {
    self->data[0] = 12;
    self->read_stream.read(self->data, TFTP_FRAME_MAX_DATA_LEN);
    std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "] bytes read :" << self->read_stream.gcount() << std::endl;
    self->frame = frame::create_data_frame(
        &self->data[0], std::min(&self->data[self->read_stream.gcount()], &self->data[TFTP_FRAME_MAX_DATA_LEN]),
        self->block_number);
    self->frame = frame::create_read_request_frame(self->filename);
    std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "]" << std::endl;
    /*auto a = self->frame->get_asio_buffer();
    auto b = self->client_endpoint;
    auto cb =  std::bind(&download_server::receiver, std::move(self), std::placeholders::_1,
                         std::placeholders::_2);*/
    self->socket.async_send_to(self->frame->get_asio_buffer(), self->client_endpoint,
                               std::bind(&download_server::receiver, std::move(self), std::placeholders::_1,
                                         std::placeholders::_2));
  }
  break;
  default:
  break;
  }
}

void download_server::receiver(download_server_u &self, const boost::system::error_code &e,
                               std::size_t bytes_sent) {
  (void)(self);
  (void)(e);
  (void)(bytes_sent);
  std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "] bytes sent :" << bytes_sent << std::endl;
}

void download_server::update_stage(const std::size_t &bytes_transacted) {
  (void)(bytes_transacted);
  std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "]" << std::endl;
  switch(this->stage){
  case ds_init:
    this->stage = ds_send_data;
  break;
  case ds_send_data:
    this->stage = ds_recv_ack;
  break;
  case ds_recv_ack:
    //If these two are not true then we need to resend same frame
    if(this->frame->get_op_code() == frame::op_ack){
      if(this->frame->get_block_number() == this->block_number){
        this->block_number++;
      }
    }
    this->stage = ds_send_data;
  break;
  case ds_exit:
  default:
  break;
  }
}

void spin_tftp_server(boost::asio::io_context &io, frame_csc &first_frame,
                      const udp::endpoint &client_endpoint) {
  switch(first_frame->get_op_code()) {
  case frame::op_read_request:
    download_server::serve(io, first_frame, client_endpoint);
    break;
  case frame::op_write_request:
    break;
  default:
    break;
  }
}


distributor::distributor(boost::asio::io_context &io, const udp::endpoint &local_endpoint, std::string &work_dir)
    : io(io), socket(io, local_endpoint), work_dir(work_dir), server_count(0) { }

distributor_s distributor::create(boost::asio::io_context &io, const udp::endpoint &local_endpoint,
                                  std::string work_dir){
  return std::make_shared<distributor>(distributor(io, local_endpoint, work_dir));
}
/*
distributor_s distributor::create(boost::asio::io_context &io, const uint16_t udp_port, std::string work_dir){
}*/

uint64_t distributor::start_service(){
  this->perform_distribution();
  return server_count;
}

uint64_t distributor::stop_service(){
  this->socket.cancel();
  return server_count;
}

void distributor::perform_distribution() {
  first_frame = frame::create_empty_frame();
  this->socket.async_receive_from(first_frame->get_asio_buffer(), this->remote_endpoint,
                                  std::bind(&distributor::perform_distribution_cb, shared_from_this(),
                                            std::placeholders::_1, std::placeholders::_2));
}

void distributor::perform_distribution_cb(const boost::system::error_code &error,
                                          const std::size_t &bytes_received) {
  if(error == boost::asio::error::operation_aborted){
    return;
  }
  first_frame->resize(bytes_received);
  first_frame->parse_frame();
  spin_tftp_server(this->io, this->first_frame, this->remote_endpoint);
  this->perform_distribution();
}

