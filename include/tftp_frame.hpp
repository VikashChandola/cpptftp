#ifndef __TFTP_FRAME_HPP__
#define __TFTP_FRAME_HPP__

//#include <cstdint>
#include <exception>
#include <map>
#include <unordered_map>
#include <vector>

#include <boost/asio/buffer.hpp>

#include "tftp_exception.hpp"

#define TFTP_FRAME_MAX_DATA_LEN 512
#define MAX_BASE_FRAME_LEN      516

namespace tftp {
class frame {
public:
  enum op_code {
    op_read_request = 0x01,
    op_write_request,
    op_data,
    op_ack,
    op_error,
    op_oack,
    // op_invalid is not actual op code. This is default opcode symbolizing
    // invalid op code This is required because it's the opcode that sets what
    // all attributes of this object are valid
    op_invalid
  };

  enum data_mode { mode_octet = 1, mode_netascii };

  enum error_code {
    undefined_error,
    file_not_found,
    access_violation,
    disk_full,
    illegal_tftp_operation,
    unknown_transfer_id,
    file_already_exists,
    no_such_user
  };

  enum option_key {
    option_blksize = 0,
  };

  static const std::size_t max_data_len = TFTP_FRAME_MAX_DATA_LEN;

  template <typename T>
  void make_data_frame(T itr, const T &itr_end, const uint16_t &block_number) {
    this->data.clear();
    this->data.push_back(0x00);
    this->data.push_back(op_data);
    this->code = op_data;
    this->code = frame::op_data;
    this->append_to_frame(block_number);
    this->block_number = block_number;
    this->append_to_frame(itr, std::min(itr_end, itr + max_data_len));
  }

  void make_ack_frame(const uint16_t &block_number) noexcept;

  void make_error_frame(const error_code &e_code, std::string error_message = "");

  void make_read_request_frame(const std::string &, const frame::data_mode &mode = mode_octet);

  void make_write_request_frame(const std::string &, const frame::data_mode &mode = mode_octet);

  void parse_frame(const op_code &expected_opcode);

  void parse_frame();

  boost::asio::mutable_buffer &get_asio_buffer_for_recv();
  boost::asio::mutable_buffer &get_asio_buffer();

  typedef std::vector<char>::const_iterator const_iterator;

  const_iterator cbegin() const { return this->data.cbegin(); }

  const_iterator cend() const { return this->data.cend(); }

  const_iterator data_cbegin() const;

  const_iterator data_cend() const { return this->cend(); }

  op_code get_op_code() const noexcept { return this->code; }

  data_mode get_data_mode() const;

  uint16_t get_block_number() const;

  error_code get_error_code() const;

  std::string get_error_message() const;

  std::string get_filename() const;

  void resize(std::size_t new_size) { this->data.resize(new_size); }

  void set_option(const option_key &key, const uint16_t value) noexcept {
    this->set_option(key, std::to_string(value));
  }

  void set_option(const option_key &key, const std::string &value) noexcept { options[key] = value; }

  bool get_option(const option_key &key, std::string &value) const noexcept;

  void clear_option(const option_key &key) { this->options.erase(key); }

  void clear_option() noexcept { this->options.clear(); }

  frame() : code(op_invalid) { this->data.resize(MAX_BASE_FRAME_LEN); }

  // No copy
  frame(frame &) = delete;
  frame &operator=(const frame &) = delete;

private:
  void make_request_frame(const op_code &, const std::string &, const frame::data_mode &);

  void append_to_frame(const data_mode &d_mode);

  void append_to_frame(const std::string &data);

  void append_to_frame(const uint16_t &data);

  void append_to_frame(const char &&data);

  template <typename T>
  void append_to_frame(T itr, const T &itr_end) {
    while (itr != itr_end) {
      this->data.push_back(*itr);
      itr++;
    }
  }

  typedef std::unordered_map<option_key, std::string> options_map;
  std::vector<char> data;
  op_code code;
  data_mode mode;
  uint16_t block_number;
  std::string file_name;
  error_code e_code;
  std::string error_message;
  boost::asio::mutable_buffer buffer;
  options_map options;
};
} // namespace tftp
#endif

/* NOTES:
 * 1. It would have been more efficient if there was one parent class frame and different kind of frames
 * example error frame, data frame, request frame etc were child classes Why, User of frame needs frame
 * specific data for example user of error frame will need a getter for error message but this method is
 * always there for frame. It becomes useless API if frame is a data frame. This leads to check in each getter
 * for frame type. Most of the public methods are not relevnat for frame object.
 * ---------------------------------------------06/20/2021---------------------------------------------
 *  2. Point 1 hasn't created any problem till now. Current way of doing may be good enough for small framing
 * as we need for current use case. So keep using single class approach until it keeps working.
 */
