#include <iostream>
#include "tftp_server.hpp"
#include "tftp_frame.hpp"

using boost::asio::ip::udp;
using namespace tftp;

server::server(boost::asio::io_context &io, frame_csc &frame, const udp::endpoint &endpoint,
               const std::string &work_dir) : socket(io), client_endpoint(endpoint),
               filename(work_dir + "/" + frame->get_filename()  ) {
  socket.open(udp::v4());
}

download_server::download_server(boost::asio::io_context &io, frame_csc &first_frame, 
                                 const udp::endpoint &endpoint, const std::string &work_dir)
    : server(io, first_frame, endpoint, work_dir), stage(ds_init),
      read_stream(this->filename, std::ios::in | std::ios::binary),
      block_number(0), is_last_frame(false) {
  std::cout << "Provisioning downloader_server object for client " << this->client_endpoint << std::endl;
}

download_server::~download_server() {
  std::cout <<"Destorying downloader_server object for client " << this->client_endpoint << std::endl;
}

void download_server::serve(boost::asio::io_context &io, frame_csc &frame, const udp::endpoint &endpoint,
                            const std::string &work_dir) {
  download_server_s self = std::make_shared<download_server>(io, frame, endpoint, work_dir);
  if(!self->read_stream.is_open()){
    std::cout << "Failed to read '" << self->filename << "'" << std::endl;
    return;
  }
  self->sender(boost::system::error_code(), 0);
}

void download_server::sender(const boost::system::error_code &e, const std::size_t &bytes_received) {
  (void)(e);
  std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "] bytes received :" << bytes_received
            << "Stage :" << this->stage << " file :" << this->filename << std::endl;
  this->update_stage(e, bytes_received);
  switch(this->stage) {
  case download_server::ds_send_data:
  {
    this->read_stream.read(this->data, TFTP_FRAME_MAX_DATA_LEN);
    if(this->read_stream.eof()) {
      std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "l eof" << std::endl;
      this->is_last_frame = true;
    }
    this->frame = frame::create_data_frame(
        &this->data[0], std::min(&this->data[this->read_stream.gcount()], &this->data[TFTP_FRAME_MAX_DATA_LEN]),
        this->block_number);
    this->socket.async_send_to(this->frame->get_asio_buffer(), this->client_endpoint,
                               std::bind(&download_server::receiver, shared_from_this(), std::placeholders::_1,
                                         std::placeholders::_2));
  }
  break;
  default:
  break;
  }
}

void download_server::receiver(const boost::system::error_code &e, const std::size_t &bytes_sent) {
  (void)(e);
  std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "] bytes sent :" << bytes_sent 
            << " error code :" << e << std::endl;
}

void download_server::update_stage(const boost::system::error_code &e, const std::size_t &bytes_transacted) {
  (void)(bytes_transacted);
  (void)(e);
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
  case ds_send_error:
  break;
  case ds_exit:
  default:
  break;
  }
}

void spin_tftp_server(boost::asio::io_context &io, frame_csc &first_frame,
                      const udp::endpoint &client_endpoint, const std::string &work_dir) {
  switch(first_frame->get_op_code()) {
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
  this->first_frame = frame::create_empty_frame();
  this->socket.async_receive_from(this->first_frame->get_asio_buffer(), this->remote_endpoint,
                                  std::bind(&distributor::perform_distribution_cb, shared_from_this(),
                                            std::placeholders::_1, std::placeholders::_2));
}

void distributor::perform_distribution_cb(const boost::system::error_code &error,
                                          const std::size_t &bytes_received) {
  if(error == boost::asio::error::operation_aborted){
    return;
  }
  this->first_frame->resize(bytes_received);
  this->first_frame->parse_frame();
  std::cout << "Received request from " << this->remote_endpoint << std::endl;
  spin_tftp_server(this->io, this->first_frame, this->remote_endpoint, this->work_dir);
  this->perform_distribution();
}



