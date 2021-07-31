#include "tftp_frame.hpp"
#include <iomanip>
#include <iostream>
void print_frame(const tftp::frame &f) {
  std::cout << ">>>> [" << std::setw(3) << f.cend() - f.cbegin() << "]";
  for (auto i = f.cbegin(); i != f.cend(); i++) {
    std::cout << "0x" << std::setfill('0') << std::setw(2) << std::hex << static_cast<int>(*i) << " ";
  }
  std::cout << std::dec << std::endl;
}
int main() {
  tftp::frame f;

  std::cout << "Creating read request frame" << std::endl;
  //f.reset();
  f.set_option("ABCDEFGHIJKLMNOPQRSTUVWYZ0", "ABCDEFGHIJKLMNOPQRSTUVWYZ0");
  /*f.set_option("ABCDEFGHIJKLMNOPQRSTUVWYZA", "ABCDEFGHIJKLMNOPQRSTUVWYZA");
  f.set_option("ABCDEFGHIJKLMNOPQRSTUVWYZB", "ABCDEFGHIJKLMNOPQRSTUVWYZB");
  f.set_option("ABCDEFGHIJKLMNOPQRSTUVWYZC", "ABCDEFGHIJKLMNOPQRSTUVWYZC");
  f.set_option("ABCDEFGHIJKLMNOPQRSTUVWYZD", "ABCDEFGHIJKLMNOPQRSTUVWYZD");
  f.set_option("ABCDEFGHIJKLMNOPQRSTUVWYZE", "ABCDEFGHIJKLMNOPQRSTUVWYZE");
  f.set_option("ABCDEFGHIJKLMNOPQRSTUVWYZF", "ABCDEFGHIJKLMNOPQRSTUVWYZF");
  f.set_option("ABCDEFGHIJKLMNOPQRSTUVWYZG", "ABCDEFGHIJKLMNOPQRSTUVWYZG");
  f.set_option("ABCDEFGHIJKLMNOPQRSTUVWYZH", "ABCDEFGHIJKLMNOPQRSTUVWYZH");
  f.set_option("ABCDEFGHIJKLMNOPQRSTUVWYZJ", "ABCDEFGHIJKLMNOPQRSTUVWYZJ");*/

  f.make_read_request_frame("somefile");
  print_frame(f);

  std::cout << "Creating write request frame" << std::endl;
  //f.reset();
  f.make_write_request_frame("somefile");
  print_frame(f);

  std::cout << "Creating write request frame" << std::endl;
  //f.reset();
  f.make_write_request_frame("somefile");
  print_frame(f);

  std::cout << "Creating ack frame" << std::endl;
  //f.reset();
  f.make_ack_frame(15);
  print_frame(f);

  std::string s("123456789");
  std::cout << "Creating data frame" << std::endl;
  //f.reset();
  f.make_data_frame(s.cbegin(), s.cend(), 15);
  print_frame(f);

  std::cout << "Done" << std::endl;
  return 0;
}
/*
#define BOOST_TEST_MODULE tftp frame

#include <algorithm>
#include <boost/asio/buffer.hpp>
#include <iostream>
#include <random>

#include <boost/test/data/monomorphic.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/included/unit_test.hpp>

#include "tftp_frame.hpp"

namespace bdata = boost::unit_test::data;

std::string get_random_filename(uint32_t filename_length) {
  std::stringstream ss;
  static bool seeded = false;
  if (seeded) {
    std::srand(std::time(nullptr));
    seeded = true;
  }
  while (filename_length-- != 0) {
    ss << static_cast<char>('A' + (std::rand() % 26));
  }
  return ss.str();
}

std::map<std::string, tftp::frame::op_code> read_write_request_dataset() {
  std::map<std::string, tftp::frame::op_code> dataset;
  for (uint32_t i = 0; i < 256; i += 17) {
    dataset.insert(std::make_pair(get_random_filename(i), tftp::frame::op_read_request));
    dataset.insert(std::make_pair(get_random_filename(i), tftp::frame::op_write_request));
  }
  return dataset;
}

typedef std::pair<const std::string, tftp::frame::op_code>
    read_write_request_dataset_log_dont_print_log_value;
BOOST_TEST_DONT_PRINT_LOG_VALUE(read_write_request_dataset_log_dont_print_log_value)

BOOST_DATA_TEST_CASE(read_write_request_frame_creation,
                     bdata::make(read_write_request_dataset()),
                     test_sample) {
  std::string filename         = test_sample.first;
  tftp::frame::op_code op_code = test_sample.second;
  tftp::frame_s f;
  try {
    if (op_code == tftp::frame::op_read_request) {
      f = tftp::frame::create_read_request_frame(filename);
    } else {
      f->make_write_request_frame(filename);
    }
  } catch (tftp::invalid_frame_parameter_exception &e) {
    if (filename.length() == 0 || filename.length() > 255) {
      BOOST_TEST(true);
      return;
    } else {
      BOOST_TEST(false);
    }
  }
  if (filename.length() == 0 || filename.length() > 255) {
    BOOST_TEST(false);
  }

  const std::vector<char> &f_data = f->get_frame_as_vector();
  auto itr                        = f_data.cbegin();

  BOOST_TEST(*itr == 0x00, "First byte of frame is not 0x00");

  itr++;
  BOOST_TEST(f->get_op_code() == op_code, "OP Code mismatch");
  BOOST_TEST(*itr == op_code, "OP Code mismatch");

  itr++;

  for (char ch : filename) {
    BOOST_TEST(*itr == ch, "filename mismatch in tftp frame");
    itr++;
  }

  BOOST_TEST(*itr == 0x00, "Separator is not 0x00");
  itr++;
  BOOST_TEST(f->get_data_mode() == tftp::frame::mode_octet, "Data mode is invalid");
  for (char ch : std::string("octet")) {
    BOOST_TEST(*itr == ch, "Data mode is invalid");
    itr++;
  }

  BOOST_TEST(*itr == 0x00, "Frame is not ending with 0x00");

  itr++;
  BOOST_TEST((itr == f_data.cend()), "Frame is longer than expected");
}

BOOST_DATA_TEST_CASE(data_frame_creation, bdata::xrange(0, 600, 50), data_length) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> distrib(0, 255);

  std::vector<char> data;
  uint16_t block_number =
      ((static_cast<uint16_t>(distrib(gen))) << 8) | (static_cast<uint16_t>(distrib(gen)));

  for (int i = 0; i < data_length; ++i) {
    data.push_back(static_cast<char>(distrib(gen)));
  }
  tftp::frame_s f                 = tftp::frame::create_data_frame(data.cbegin(), data.cend(), block_number);
  const std::vector<char> &f_data = f->get_frame_as_vector();
  BOOST_TEST(f_data.size() >= 4, "Data frame is smaller than 4 bytes");

  auto itr = f_data.cbegin();
  BOOST_TEST(*itr == 0x00, "First byte of data frame is not 0x00");
  itr++;
  BOOST_TEST(*itr == tftp::frame::op_data, "Incorrect op code in data frame");

  itr++;
  BOOST_TEST(f->get_block_number() == block_number, "Data frame block number mismatch");
  BOOST_TEST(*itr == static_cast<char>(block_number >> 8), "Data frame block number mismatch");
  itr++;
  BOOST_TEST(*itr == static_cast<char>(block_number), "Data frame block number mismatch");

  itr++;
  BOOST_TEST((f_data.cend() >= itr), "Invalid data in data frame");
  for (std::size_t i = 0; i < std::min(data.size(), static_cast<std::size_t>(TFTP_FRAME_MAX_DATA_LEN)); i++) {
    BOOST_TEST(*itr == data[i], "Data mismatch in data frame");
    itr++;
  }
  BOOST_TEST((itr == f_data.cend()), "Invalid end for data frame");
}

BOOST_DATA_TEST_CASE(ack_frame_creation, bdata::make({0, 1, 255, 1000, 65535}), block_number) {
  tftp::frame_s f                 = tftp::frame::create_ack_frame(block_number);
  const std::vector<char> &f_data = f->get_frame_as_vector();

  auto itr                        = f_data.cbegin();
  BOOST_TEST(*itr == 0x00, "First byte of data frame is not 0x00");

  itr++;
  BOOST_TEST(*itr == tftp::frame::op_ack, "Incorrect op code for ack frame");

  itr++;
  BOOST_TEST(f->get_block_number() == block_number, "Data frame block number mismatch");
  BOOST_TEST(*itr == static_cast<char>(block_number >> 8), "Data frame block number mismatch");
  itr++;
  BOOST_TEST(*itr == static_cast<char>(block_number), "Data frame block number mismatch");

  itr++;
  BOOST_TEST((itr == f_data.cend()), "Invalid end for ack frame");
}

std::map<tftp::frame::error_code, std::string> error_frame_dataset() {
  std::map<tftp::frame::error_code, std::string> dataset;
  for (uint32_t i = 0; i < 8; i++) {
    dataset.insert(std::make_pair(static_cast<tftp::frame::error_code>(i), std::string()));
    dataset.insert(std::make_pair(static_cast<tftp::frame::error_code>(i), std::string("Blah blih balh")));
  }
  return dataset;
}

typedef std::pair<const tftp::frame::error_code, std::string> error_frame_dataset_dont_print_log_value;
BOOST_TEST_DONT_PRINT_LOG_VALUE(error_frame_dataset_dont_print_log_value)

BOOST_DATA_TEST_CASE(error_frame_creation, bdata::make(error_frame_dataset()), test_sample) {
  std::string error_message          = test_sample.second;
  tftp::frame::error_code error_code = test_sample.first;

  tftp::frame_s f;
  if (error_message.empty()) {
    f = tftp::frame::create_error_frame(error_code);
  } else {
    f = tftp::frame::create_error_frame(error_code, error_message);
  }

  const std::vector<char> &f_data = f->get_frame_as_vector();

  auto itr                        = f_data.cbegin();
  BOOST_TEST(*itr == 0x00, "First byte of data frame is not 0x00");

  itr++;
  BOOST_TEST(*itr == tftp::frame::op_error, "Incorrect op code for error frame");

  itr++;
  BOOST_TEST((f->get_error_code() == error_code), "Error code mismatch");
  BOOST_TEST(*itr == static_cast<char>(static_cast<uint16_t>(error_code) >> 8), "Error code mismatch");
  itr++;
  BOOST_TEST(*itr == static_cast<char>(static_cast<uint16_t>(error_code)), "Error code mismatch");

  itr++;
  for (char ch : error_message) {
    BOOST_TEST(*itr == ch, "Error message is not valid");
    itr++;
  }

  itr++;
  if (!error_message.empty()) {
    BOOST_TEST((itr == f_data.cend()), "Invalid end for ack frame");
  }
  BOOST_TEST(*(f_data.cend() - 1) == 0x00, "Last byte of ack frame is not 0x00");
}
*/
