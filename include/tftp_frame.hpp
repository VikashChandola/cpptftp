#ifndef __TFTP_FRAME_HPP__
#define __TFTP_FRAME_HPP__

#include <cstdint>
#include <exception>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include <boost/asio/buffer.hpp>

#include "tftp_exception.hpp"

/* This module is responsible for managing tftp frames. New frame is meant to be
 * created for each tftp transaction. tftpframe module can be used for parsing
 * packets received as well as creating new tftp frames.
 *
 * Usage Instructions for creating frames. This is typically used for creating a
 * frame and transmitting to remote end. create_.*_frame methods should be used
 * for creating frame of specific kind. User can then get underlying buffer
 * using get_asio_buffer() method for transmitting data. Example frame_s f =
 * create_read_request_frame("my_file") async_send_data(f->get_asio_buffer(),
 * cb);
 *
 * Usage instruction for parsing frames.
 * This is typically used for parsing received frames. create_empty_frame method
 * creates an empty frame which can be used for reception of data from remote
 * end. Once data is received user must request frame to resize to number of
 * bytes received and then call parse_frame method
 * f = create_empty_frame();
 * async_recv_data(f->get_asio_buffer(), cb);
 * ...
 * void cb(bytes_received, ...){
 *   f->resize(bytes_received);
 *   f->parse_frame();
 *   //object f is usable now.
 * }
 */
#define TFTP_FRAME_MAX_DATA_LEN 512

namespace tftp {

class frame;
typedef std::shared_ptr<frame> frame_s;
class frame {
public:
  enum op_code {
    op_read_request = 0x01,
    op_write_request,
    op_data,
    op_ack,
    op_error,
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
    file_exists,
    no_such_user
  };

  static const std::size_t max_data_len = TFTP_FRAME_MAX_DATA_LEN;

  static frame_s create_read_request_frame(const std::string &file_name, const data_mode mode = mode_octet);

  static frame_s create_write_request_frame(const std::string &file_name, const data_mode mode = mode_octet);

  static frame_s create_data_frame(std::vector<char>::const_iterator itr,
                                   const std::vector<char>::const_iterator &itr_end, const uint16_t &block_number,
                                   const std::size_t frame_size = max_data_len);

  static frame_s create_ack_frame(const uint16_t &block_number);

  static frame_s create_error_frame(const error_code &e_code, const std::string &error_message = "");

  static frame_s create_empty_frame();

  void parse_frame();

  const std::vector<char> &get_frame_as_vector() { return this->data; }

  boost::asio::mutable_buffer &get_asio_buffer();

  std::pair<std::vector<char>::const_iterator, std::vector<char>::const_iterator> get_data_iterator();

  op_code get_op_code() { return this->code; }

  data_mode get_data_mode();

  uint16_t get_block_number();

  void resize(std::size_t new_size) { this->data.resize(new_size); }

private:
  frame() : code(op_invalid) {}

  static frame_s get_base_frame(op_code code = op_invalid);

  void append_to_frame(const data_mode &d_mode);

  void append_to_frame(const error_code &e_code);

  void append_to_frame(const std::string &data);

  void append_to_frame(const uint16_t &data);

  void append_to_frame(const char &&data);

  template <typename T> void append_to_frame(T itr, const T &itr_end);

  std::vector<char> data;
  op_code code;
  data_mode mode;
  uint16_t block_number;
  std::string file_name;
  error_code e_code;
  std::string error_message;
  boost::asio::mutable_buffer buffer;
};

} // namespace tftp
#endif
