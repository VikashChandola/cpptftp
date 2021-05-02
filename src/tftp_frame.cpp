#include <cstdint>
#include <exception>
#include <map>
#include <memory>
#include <vector>

#include "tftp_exception.hpp"
#include "tftp_frame.hpp"

using namespace tftp;

static std::map<frame::data_mode, std::string> data_mode_map{
    {frame::mode_octet, "octet"},
    //{frame::mode_netascii, "netascii"},
};

static std::map<frame::error_code, std::string> error_code_map{
    {frame::undefined_error, "Undefined. Please check error message(if any)"},
    {frame::file_not_found, "File not found"},
    {frame::access_violation, "Access violation"},
    {frame::disk_full, "Disk full or allocation exceeded"},
    {frame::illegal_tftp_operation, "Illegal TFTP operation."},
    {frame::unknown_transfer_id, "Unknown transfer ID."},
    {frame::file_exists, "File already exists."},
    {frame::no_such_user, "No such user."},
};

frame_s frame::create_read_request_frame(const std::string &file_name, const frame::data_mode mode) {
  if (file_name.size() > 255 || file_name.size() < 1){
    throw invalid_frame_parameter_exception(
        "Read request with filename larger than 255 characters or smaller than 1");
  }
  frame_s self = frame::get_base_frame(frame::op_read_request);
  self->code = frame::op_read_request;
  self->append_to_frame(file_name);
  self->file_name = file_name;
  self->append_to_frame(0x00);
  self->append_to_frame(mode);
  self->mode = mode;
  self->append_to_frame(0x00);
  return self;
}

frame_s frame::create_write_request_frame(const std::string &file_name, const frame::data_mode mode) {
  frame_s self = create_read_request_frame(file_name, mode);
  self->data[1] = frame::op_write_request;
  self->code = frame::op_write_request;
  return self;
}

frame_s frame::create_data_frame(std::vector<char>::const_iterator itr,
                                 const std::vector<char>::const_iterator &itr_end, const uint16_t &block_number,
                                 const std::size_t frame_size) {
  if (frame_size > 512) {
    throw framing_exception("Frame data larger than 512 byes");
  }
  frame_s self = frame::get_base_frame(frame::op_data);
  self->code = frame::op_data;
  self->append_to_frame(block_number);
  self->block_number = block_number;
  self->append_to_frame(itr, std::min(itr_end, itr + frame_size));
  return self;
}

frame_s frame::create_ack_frame(const uint16_t &block_number) {
  frame_s self = frame::get_base_frame(op_ack);
  self->code = op_ack;
  self->append_to_frame(block_number);
  self->block_number = block_number;
  return self;
}

frame_s frame::create_error_frame(const frame::error_code &e_code, const std::string &error_message) {
  frame_s self = frame::get_base_frame(op_error);
  self->code = op_error;
  self->append_to_frame(e_code);
  self->e_code = e_code;
  self->append_to_frame(error_message);
  self->error_message = error_message;
  self->append_to_frame(0x00);
  return self;
}

frame_s frame::create_empty_frame() {
  auto empty_frame = frame::get_base_frame();
  empty_frame->data.resize(516);
  return empty_frame;
}

void frame::parse_frame() {
  if (this->data.size() < 4) {
    throw framing_exception("Can't parse frame with length smaller than 4");
  }
  auto itr = this->data.cbegin();
  if (*itr != 0x00) {
    throw invalid_frame_parameter_exception("Invalid OP code");
  }
  itr++;
  this->code = static_cast<op_code>(*itr);
  itr++;
  switch (this->code) {
  case op_read_request: {
    throw missing_feature_exception("Feature Not implemented");
  } break;
  case op_write_request: {
    throw missing_feature_exception("Feature Not implemented");
  } break;
  case op_data: {
    if (itr + 2 > this->data.cend()) {
      throw partial_frame_exception("No block number in packet");
    }
    this->block_number = (static_cast<uint16_t>(*itr) << 8) + (static_cast<uint16_t>(*(itr + 1)));
  } break;
  case op_ack: {
    throw missing_feature_exception("op ack feature not implemented");
  } break;
  case op_error: {
    throw missing_feature_exception("op error feature not implemented");
  } break;
  default: {
    throw invalid_frame_parameter_exception("Invalid OP code");
  } break;
  }
}

boost::asio::mutable_buffer &frame::get_asio_buffer() {
  this->buffer = boost::asio::buffer(this->data);
  return this->buffer;
}

std::pair<std::vector<char>::const_iterator, std::vector<char>::const_iterator> frame::get_data_iterator() {
  if (this->code != op_data) {
    throw framing_exception("Not a data frame");
  }
  if (this->data.size() < 4) {
    throw partial_frame_exception("Data frame smaller than 4 bytes");
  }
  return std::make_pair(this->data.cbegin() + 4, this->data.cend());
}

frame::data_mode frame::get_data_mode() {
  if (this->code == op_data || this->code == op_read_request || this->code == op_write_request) {
    return this->mode;
  }
  throw invalid_frame_parameter_exception("Mode can't be provided. Not a data, read request or write request "
                                               "frame");
}

uint16_t frame::get_block_number() {
  if (this->code == op_data || this->code == op_ack) {
    return this->block_number;
  }
  throw invalid_frame_parameter_exception("block number can't be provided. Not a data or ack frame");
}

// PRIVATE data members

frame_s frame::get_base_frame(frame::op_code code) {
  frame_s self(new frame()); // can't use std::make_shared<frame>();
  if (code == op_invalid) {
    return self;
  }
  self->data.push_back(0x00);
  self->data.push_back(code);
  self->code = code;
  return self;
}

void frame::append_to_frame(const frame::data_mode &d_mode) { this->append_to_frame(data_mode_map[d_mode]); }

void frame::append_to_frame(const frame::error_code &e_code) { this->append_to_frame(error_code_map[e_code]); }

void frame::append_to_frame(const std::string &data) { this->append_to_frame(data.cbegin(), data.cend()); }

void frame::append_to_frame(const uint16_t &data) {
  this->data.push_back(static_cast<char>(data >> 8));
  this->data.push_back(static_cast<char>(data));
}

void frame::append_to_frame(const char &&data) { this->data.push_back(static_cast<char>(data)); }

template <typename T> void frame::append_to_frame(T itr, const T &itr_end) {
  while (itr != itr_end) {
    this->data.push_back(*itr);
    itr++;
  }
}
