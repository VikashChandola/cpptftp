#include <exception>
#include <map>
#include <memory>
#include <vector>
#include <cstdint>

#include "tftp_exception.hpp"
#include "tftp_frame.hpp"

static std::map<tftp_frame::data_mode, std::string> data_mode_map{
    {tftp_frame::mode_octet, "octet"},
    {tftp_frame::mode_netascii, "netascii"},
};

static std::map<tftp_frame::error_code, std::string> error_code_map{
    {tftp_frame::undefined_error, "Undefined. Please check error message(if any)"},
    {tftp_frame::file_not_found, "File not found"},
    {tftp_frame::access_violation, "Access violation"},
    {tftp_frame::disk_full, "Disk full or allocation exceeded"},
    {tftp_frame::illegal_tftp_operation, "Illegal TFTP operation."},
    {tftp_frame::unknown_transfer_id, "Unknown transfer ID."},
    {tftp_frame::file_exists, "File already exists."},
    {tftp_frame::no_such_user, "No such user."},
};

tftp_frame_s
tftp_frame::create_read_request_frame(const std::string &file_name,
                                      const tftp_frame::data_mode mode) {
  tftp_frame_s self = tftp_frame::get_base_frame(tftp_frame::op_read_request);
  self->code = tftp_frame::op_read_request;
  self->append_to_frame(file_name);
  self->file_name = file_name;
  self->append_to_frame(0x00);
  self->append_to_frame(mode);
  self->mode = mode;
  return self;
}

tftp_frame_s
tftp_frame::create_write_request_frame(const std::string &file_name,
                                       const tftp_frame::data_mode mode) {
  tftp_frame_s self = create_read_request_frame(file_name, mode);
  self->frame[1] = tftp_frame::op_write_request;
  self->code = tftp_frame::op_write_request;
  return self;
}

tftp_frame_s
tftp_frame::create_data_frame(std::vector<char>::const_iterator itr,
                              const std::vector<char>::const_iterator &itr_end,
                              const uint16_t &block_number,
                              const std::size_t frame_size) {
  if (frame_size > 512){
    throw tftp_framing_exception("Frame data larger than 512 byes");
  }
  tftp_frame_s self = tftp_frame::get_base_frame(tftp_frame::op_data);
  self->code = tftp_frame::op_data;
  self->append_to_frame(block_number);
  self->block_number = block_number;
  self->append_to_frame(itr, std::min(itr_end, itr + frame_size));
  return self;
}

tftp_frame_s tftp_frame::create_ack_frame(const uint16_t &block_number) {
  tftp_frame_s self = tftp_frame::get_base_frame(op_ack);
  self->code = op_ack;
  self->append_to_frame(block_number);
  self->block_number = block_number;
  return self;
}

tftp_frame_s
tftp_frame::create_error_frame(const tftp_frame::error_code &e_code,
                               const std::string &error_message) {
  tftp_frame_s self = tftp_frame::get_base_frame(op_error);
  self->code = op_error;
  self->append_to_frame(e_code);
  self->e_code = e_code;
  self->append_to_frame(error_message);
  self->error_message = error_message;
  return self;
}

void
tftp_frame::parse_frame() {
  auto itr = this->frame.cbegin();
  if (*itr != 0x00) {
    throw tftp_invalid_frame_parameter_exception("Invalid OP code");
  }
  itr++;
  this->code = static_cast<op_code>(*itr);
  switch (this->code) {
  case op_read_request: {
    throw tftp_missing_feature_exception("Feature Not implemented");
  } break;
  case op_write_request: {
    throw tftp_missing_feature_exception("Feature Not implemented");
  } break;
  case op_data: {
    if (itr + 2 >= this->frame.cend()) {
      throw tftp_partial_frame_exception("No block number in packet");
    }
    this->block_number = (static_cast<uint16_t>(*itr) << 8) +
                         (static_cast<uint16_t>(*(itr + 1)));
  } break;
  case op_ack: {
    throw tftp_missing_feature_exception("op ack feature not implemented");
  } break;
  case op_error: {
    throw tftp_missing_feature_exception("op error feature not implemented");
  } break;
  default: {
    throw tftp_invalid_frame_parameter_exception("Invalid OP code");
  } break;
  }
}

boost::asio::mutable_buffer& tftp_frame::get_asio_buffer() {
  this->buffer = boost::asio::buffer(this->frame);
  return this->buffer;
}


std::pair<std::vector<char>::const_iterator, std::vector<char>::const_iterator>
tftp_frame::get_data_iterator() {
  if (this->code != op_data) {
    throw tftp_framing_exception("Not a data frame");
  }
  if (this->frame.size() < 4) {
    throw tftp_partial_frame_exception("Data frame smaller than 4 bytes");
  }
  return std::make_pair(this->frame.cbegin() + 4, this->frame.cend());
}

tftp_frame::data_mode tftp_frame::get_data_mode() {
  if (this->code == op_data || this->code == op_read_request || this->code == op_write_request) {
    return this->mode;
  }
  throw tftp_invalid_frame_parameter_exception("Mode can't be provided. Not a data, read request or write request frame");
}

uint16_t tftp_frame::get_block_number() {
  if (this->code == op_data || this->code == op_ack) {
    return this->block_number;
  }
  throw tftp_invalid_frame_parameter_exception("block number can't be provided. Not a data or ack frame");
}

// PRIVATE data members

tftp_frame_s tftp_frame::get_base_frame(tftp_frame::op_code code) {
  tftp_frame_s self(new tftp_frame()); //= std::make_shared<tftp_frame>();
  if (code == op_invalid) {
    return self;
  }
  self->frame.push_back(0x00);
  self->frame.push_back(code);
  self->code = code;
  return self;
}

void tftp_frame::append_to_frame(const tftp_frame::data_mode &d_mode) {
  this->append_to_frame(data_mode_map[d_mode]);
}

void tftp_frame::append_to_frame(const tftp_frame::error_code &e_code) {
  this->append_to_frame(error_code_map[e_code]);
}

void tftp_frame::append_to_frame(const std::string &data) {
  this->append_to_frame(data.cbegin(), data.cend());
}

void tftp_frame::append_to_frame(const uint16_t &data) {
  this->frame.push_back(static_cast<char>(data >> 8));
  this->frame.push_back(static_cast<char>(data));
}

void tftp_frame::append_to_frame(const char &&data){
  this->frame.push_back(static_cast<char>(data));
}

template<typename T>
void tftp_frame::append_to_frame(
    T itr,
    const T &itr_end) {
  while (itr != itr_end) {
    this->frame.push_back(*itr);
    itr++;
  }
}
