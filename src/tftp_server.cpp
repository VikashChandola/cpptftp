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

download_server::download_server(boost::asio::io_context &io, frame_csc &first_frame, 
                                 const udp::endpoint &endpoint, const std::string &work_dir)
    : server(io, first_frame, endpoint, work_dir), stage(ds_send_data),
      read_stream(this->filename, std::ios::in | std::ios::binary), block_number(0), is_last_frame(false) {
  std::cout << "Provisioning downloader_server object for client " << this->client_endpoint << std::endl;
}

download_server::~download_server() {
  std::cout << "Destorying downloader_server object for client " << this->client_endpoint << std::endl;
}

void download_server::serve(boost::asio::io_context &io, frame_csc &frame, const udp::endpoint &endpoint,
                            const std::string &work_dir) {
  download_server_s self = std::make_shared<download_server>(io, frame, endpoint, work_dir);
  if (!self->read_stream.is_open()) {
    std::cout << "Failed to read '" << self->filename << "'" << std::endl;
    return;
  }
  self->sender();
}

void download_server::sender() {
  std::cout << __func__ << " " << this->client_endpoint << " Stage :" << this->stage << std::endl;
  switch (this->stage) {
  case download_server::ds_send_data: {
    if(this->fill_data_buffer() == false) {
      this->stage = ds_send_error;
      this->sender();
      return;
    }
    this->frame = frame::create_data_frame(
        &this->data[0], std::min(&this->data[this->data_size], &this->data[TFTP_FRAME_MAX_DATA_LEN]),
        this->block_number);
    this->socket.async_send_to(
        this->frame->get_asio_buffer(), this->client_endpoint,
        std::bind(&download_server::sender_cb, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
  } break;
  default:
    break;
  }
}

bool download_server::fill_data_buffer(){
  this->read_stream.read(this->data, TFTP_FRAME_MAX_DATA_LEN);
  this->data_size = this->read_stream.gcount();
  if (this->read_stream.eof() || this->data_size < TFTP_FRAME_MAX_DATA_LEN) {
    this->is_last_frame = true;
  }
  return true;
}

void download_server::sender_cb(const boost::system::error_code &e, const std::size_t &bytes_received) {
  (void)(e);
  (void)(bytes_received);
}

void download_server::receiver() {
}

void download_server::receiver_cb(const boost::system::error_code &e, const std::size_t &bytes_sent){
  (void)(e);
  (void)(bytes_sent);
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
  this->perform_distribution();
  return server_count;
}

uint64_t distributor::stop_service() {
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
