#include <cstdint>
#include <exception>
#include <map>
#include <memory>
#include <sstream>
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
    {frame::file_already_exists, "File already exists."},
    {frame::no_such_user, "No such user."},
};

static std::map<frame::option_key, std::string> option_key_map{
    {frame::option_blksize, "blksize"},
};

void frame::make_read_request_frame(const std::string &_file_name, const frame::data_mode &mode) {
  this->make_request_frame(frame::op_read_request, _file_name, mode);
}

void frame::make_write_request_frame(const std::string &_file_name, const frame::data_mode &mode) {
  this->make_request_frame(frame::op_write_request, _file_name, mode);
}

void frame::make_request_frame(const op_code &rq_code,
                               const std::string &_file_name,
                               const frame::data_mode &mode) {
  if (_file_name.size() > 255 || _file_name.size() < 1) {
    throw invalid_frame_parameter_exception("file name is length is not in in range [1,255]");
  }
  this->data.clear();
  this->data.push_back(0x00);
  this->data.push_back(rq_code);
  this->code = rq_code;
  this->append_to_frame(_file_name);
  this->file_name = _file_name;
  this->append_to_frame(0x00);
  this->append_to_frame(mode);
  this->mode = mode;
  for (const auto &[key, value] : this->options) {
    this->append_to_frame(0x00);
    this->append_to_frame(option_key_map.at(key));
    this->append_to_frame(0x00);
    this->append_to_frame(value);
  }
  this->append_to_frame(0x00);
}

void frame::make_ack_frame(const uint16_t &block_number) noexcept {
  this->data.clear();
  this->data.push_back(0x00);
  this->data.push_back(op_ack);
  this->code = op_ack;
  this->append_to_frame(block_number);
  this->block_number = block_number;
}

void frame::make_error_frame(const frame::error_code &e_code, std::string error_message) {
  this->data.clear();
  this->data.push_back(0x00);
  this->data.push_back(op_error);
  this->code = op_error;
  this->append_to_frame(static_cast<uint16_t>(e_code));
  this->e_code = e_code;
  if (error_message.empty()) {
    if (error_code_map.find(e_code) != error_code_map.end()) {
      error_message = error_code_map.at(e_code);
    } else {
      error_message = "Unknown error occured";
    }
  }
  this->append_to_frame(error_message);
  this->error_message = error_message;
  this->append_to_frame(0x00);
}

void frame::parse_frame(const op_code &expected_opcode) {
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
  case op_read_request:
  case op_write_request: {
    std::stringstream ss;
    while (*itr != 0x00 && itr < this->data.cend()) {
      ss << static_cast<char>(*itr);
      itr++;
    }
    this->file_name = ss.str();
    // one can parse mode from here, but who cares for mode
  } break;
  case op_data: {
    if (itr + 2 > this->data.cend()) {
      throw partial_frame_exception("No block number in packet");
    }
    this->block_number = (static_cast<uint16_t>(static_cast<uint8_t>(*itr)) << 8) +
                         (static_cast<uint16_t>(static_cast<uint8_t>(*(itr + 1))));
  } break;
  case op_ack: {
    this->block_number = (static_cast<uint16_t>(static_cast<uint8_t>(*itr)) << 8) +
                         (static_cast<uint16_t>(static_cast<uint8_t>(*(itr + 1))));
  } break;
  case op_error: {
    this->e_code = static_cast<frame::error_code>((static_cast<uint16_t>(static_cast<uint8_t>(*itr)) << 8) +
                                                  (static_cast<uint16_t>(static_cast<uint8_t>(*(itr + 1)))));
  } break;
  default: {
    throw invalid_frame_parameter_exception("Invalid OP code");
  } break;
  }
  if (expected_opcode != op_invalid && this->code != expected_opcode) {
    std::stringstream ss;
    ss << "Expected op code" << expected_opcode << " Got " << this->code;
    throw frame_type_mismatch_exception(ss.str());
  }
}

boost::asio::mutable_buffer &frame::get_asio_buffer_for_recv() {
  // this->buffer = boost::asio::mutable_buffer(&this->data[0], this->data.capacity()* sizeof(data[0]));
  this->resize(MAX_BASE_FRAME_LEN);
  return this->get_asio_buffer();
  // return this->buffer;
}

boost::asio::mutable_buffer &frame::get_asio_buffer() {
  // this->buffer = boost::asio::buffer(&this->data[0], this->data.capacity());
  this->buffer = boost::asio::mutable_buffer(&this->data[0], this->data.size() * sizeof(data[0]));
  // Do frame even need to keep a reference to buffer ?
  return this->buffer;
}

frame::const_iterator frame::data_cbegin() const {
  if (this->code == op_data) {
    // 4 bytes of header for data frame
    return 4 + this->data.cbegin();
  }
  throw invalid_frame_parameter_exception("data iterator is only available for data frame");
}

frame::data_mode frame::get_data_mode() const {
  if (this->code == op_data || this->code == op_read_request || this->code == op_write_request) {
    return this->mode;
  }
  throw invalid_frame_parameter_exception("Mode can't be provided. Not a data, read request or write request "
                                          "frame");
}

uint16_t frame::get_block_number() const {
  if (this->code == op_data || this->code == op_ack) {
    return this->block_number;
  }
  throw invalid_frame_parameter_exception("block number can't be provided. Not a data or ack frame");
}

frame::error_code frame::get_error_code() const {
  if (this->code == op_error) {
    return this->e_code;
  }
  throw invalid_frame_parameter_exception("Error code can't be provided. Not a error frame");
}

std::string frame::get_error_message() const {
  if (this->code == op_error) {
    std::stringstream ss;
    for (auto ch = this->data.cbegin() + 4; ch < this->data.cend() - 1; ch++) {
      ss << *ch;
    }
    return ss.str();
  }
  throw invalid_frame_parameter_exception("Error message can't be provided. Not a error frame");
}

std::string frame::get_filename() const {
  if (this->code == op_read_request || this->code == op_write_request) {
    return this->file_name;
  }
  throw invalid_frame_parameter_exception("file name can't be provided. Not a read/write request frame");
}

bool frame::get_option(const option_key &key, std::string &value) const noexcept {
  if (this->options.find(key) == this->options.end()) {
    return false;
  }
  value = this->options.at(key);
  return true;
}

// PRIVATE data members

void frame::append_to_frame(const frame::data_mode &d_mode) { this->append_to_frame(data_mode_map[d_mode]); }

void frame::append_to_frame(const std::string &data) { this->append_to_frame(data.cbegin(), data.cend()); }

void frame::append_to_frame(const uint16_t &data) {
  this->data.push_back(static_cast<char>(data >> 8));
  this->data.push_back(static_cast<char>(data));
}

void frame::append_to_frame(const char &&data) { this->data.push_back(static_cast<char>(data)); }
