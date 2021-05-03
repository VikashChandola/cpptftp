#define BOOST_TEST_MODULE tftp frame

#include <iostream>
#include <cstdlib>
#include <ctime>
#include <boost/asio/buffer.hpp>

#include <boost/test/included/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include "tftp_frame.hpp"

namespace bdata = boost::unit_test::data;

std::string get_random_filename(uint32_t filename_length){
  std::stringstream ss;
  static bool seeded = false;
  if(seeded){
    std::srand(std::time(nullptr));
    seeded = true;
  }
  while(filename_length-- != 0){
    ss << static_cast<char>('A' + (std::rand() % 26));
  }
  return ss.str();
}

std::map<std::string, tftp::frame::op_code> read_write_request_dataset() {
  std::map<std::string, tftp::frame::op_code> dataset;
  for(uint32_t i = 0; i < 256; i+= 17){
    dataset.insert(std::make_pair(get_random_filename(i), tftp::frame::op_read_request));
    dataset.insert(std::make_pair(get_random_filename(i), tftp::frame::op_write_request));
  }
  std::cout << dataset.size() << std::endl;
  return dataset;
}

typedef std::pair<const std::string, tftp::frame::op_code> pair_map_t;
BOOST_TEST_DONT_PRINT_LOG_VALUE(pair_map_t)

BOOST_DATA_TEST_CASE(
      read_write_request_creation,
      bdata::make(read_write_request_dataset()),
      test_sample)
{
  std::string filename = test_sample.first;
  tftp::frame::op_code op_code = test_sample.second;
  //std::cout << filename << ", " << op_code << std::endl;
  tftp::frame_s f;
  try {
    if(op_code == tftp::frame::op_read_request){
      f = tftp::frame::create_read_request_frame(filename);
    } else {
      f = tftp::frame::create_write_request_frame(filename);
    }
  } catch (tftp::invalid_frame_parameter_exception &e){
    if(filename.length() == 0 || filename.length() > 255){
      BOOST_TEST(true);
      return;
    } else {
      BOOST_TEST(false);
    }
  }
  if(filename.length() == 0 || filename.length() > 255){
    BOOST_TEST(false);
  }

  const std::vector<char>& f_data = f->get_frame_as_vector();
  auto itr = f_data.cbegin();

  BOOST_TEST(*itr == 0x00, "First byte of frame is not 0x00");

  itr++;
  BOOST_TEST(f->get_op_code() == op_code, "OP Code mismatch");
  BOOST_TEST(*itr == op_code, "OP Code mismatch");

  itr++;

  for(char ch : filename){
    BOOST_TEST(*itr == ch, "filename mismatch in tftp frame");
    itr++;
  }

  BOOST_TEST(*itr == 0x00, "Separator is not 0x00");
  itr++;
  BOOST_TEST(f->get_data_mode() == tftp::frame::mode_octet, "Data mode is invalid");
  for(char ch : std::string("octet")){
    BOOST_TEST(*itr == ch, "Data mode is invalid");
    itr++;
  }
  
  BOOST_TEST(*itr == 0x00, "Frame is not ending with 0x00");
  
  itr++;
  BOOST_TEST((itr == f_data.cend()), "Frame is longer than expected");
}

