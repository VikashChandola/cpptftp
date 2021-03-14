#include <sstream>
#include <algorithm>
#include <iomanip>

#include <boost/asio/buffer.hpp>

#include "test.hpp"
#include "tftp_frame.hpp"

static void print_vector(const std::vector<char> & v){
  for(auto itr = v.cbegin(); itr != v.cend(); itr++){
    std::cout << "0x"  << std::hex  << std::setw(2) << std::setfill('0') <<
                 (0x000000FF & static_cast<int>(*itr)) << ", ";
  }
  std::cout << std::endl;
}

// Test 001
//test read request frame creation
static TEST_RESULT validate_read_write_frame(tftp_frame_s frame, const std::string &file_name, 
                                             tftp_frame::op_code code, tftp_frame::data_mode mode,
                                             const std::vector<char> &frame_vector){
  //const std::vector<char> &frame_vector = frame->get_frame_as_vector();
  print_vector(frame_vector);
  ASSERT(frame_vector.size() > 2, "Too small frame");
  auto itr = frame_vector.cbegin();
  ASSERT(*itr == 0x00, "First byte of frame is not 0x00");
  itr++;

  ASSERT(frame->get_op_code() == code, "Invalid op code, not read/write request");
  ASSERT(*itr == code, "Invalid op code, not read/write request");
  itr++;

  {
    auto f_itr = file_name.cbegin();
    while(f_itr != file_name.cend() && itr != frame_vector.cend() && *itr != 0x00 && *itr == *f_itr){
      f_itr++;
      itr++;
    }
    ASSERT(f_itr == file_name.cend(), "Invalid filename in frame");
    ASSERT(itr != frame_vector.cend(), "Incomplete request frame");
    ASSERT(*itr == 0x00, "Invalid separator in request frame");
    itr++;
  }

  {
    ASSERT(frame->get_data_mode() == mode, "Invalid mode");
    std::string mode_string;
    switch(mode) {
      case tftp_frame::mode_octet:
        mode_string = "octet";
      break;
      case tftp_frame::mode_netascii:
        mode_string = "netascii";
      break;
    }
    auto mode_itr = mode_string.cbegin();
    while(mode_itr != mode_string.cend() && itr != frame_vector.cend() && *itr != 0x00 && *itr == *mode_itr){
      mode_itr++;
      itr++;
    }
    ASSERT(mode_itr == mode_string.cend(), "Invalid mode");
    ASSERT(itr == frame_vector.cend(), "Too long frame");
    ASSERT(*itr == 0x00, "Invalid separator");
  }
  return TEST_PASS;
}

TEST_RESULT test_read_request_frame_creation(){
  TEST_BEGIN;
  std::string filename_1("12345");
  //std::string filename_2("012345");

  tftp_frame_s req_1 = tftp_frame::create_read_request_frame(filename_1);
  CALL(validate_read_write_frame, req_1, filename_1, tftp_frame::op_read_request, 
       tftp_frame::mode_octet, req_1->get_frame_as_vector());
  STATUS("Read request 1");

  tftp_frame_s req_2 = tftp_frame::create_read_request_frame(filename_1, tftp_frame::mode_octet);
  CALL(validate_read_write_frame, req_2, filename_1, tftp_frame::op_read_request,
       tftp_frame::mode_octet, req_2->get_frame_as_vector());
  STATUS("Read request 2");

  tftp_frame_s req_3 = tftp_frame::create_read_request_frame(filename_1, tftp_frame::mode_netascii);
  CALL(validate_read_write_frame, req_3, filename_1, tftp_frame::op_read_request, 
       tftp_frame::mode_netascii, req_3->get_frame_as_vector());
  STATUS("Read request 3");

  TEST_END;
}

//test creation of write request frame
TEST_RESULT test_write_request_frame_creation(){
  TEST_BEGIN;
  std::string filename_1("12345");
  //std::string filename_2("012345");

  tftp_frame_s req_1 = tftp_frame::create_write_request_frame(filename_1);
  CALL(validate_read_write_frame, req_1, filename_1, tftp_frame::op_write_request,
       tftp_frame::mode_octet, req_1->get_frame_as_vector());
  STATUS("Write request 1");

  tftp_frame_s req_2 = tftp_frame::create_write_request_frame(filename_1, tftp_frame::mode_octet);
  CALL(validate_read_write_frame, req_2, filename_1, tftp_frame::op_write_request,
       tftp_frame::mode_octet, req_2->get_frame_as_vector());
  STATUS("Write request 2");

  tftp_frame_s req_3 = tftp_frame::create_write_request_frame(filename_1, tftp_frame::mode_netascii);
  CALL(validate_read_write_frame, req_3, filename_1, tftp_frame::op_write_request, 
       tftp_frame::mode_netascii, req_3->get_frame_as_vector());
  STATUS("Write request 3");


  //const std::vector<char> &req1_vector = req_1->get_frame_as_vector();
  //const std::vector<char> &req2_vector = req_2->get_frame_as_vector();
  //const std::vector<char> &req3_vector = req_3->get_frame_as_vector();
  TEST_END;
}

//test creation of data request frame
static TEST_RESULT validate_data_frame(tftp_frame_s frame, const std::vector<char> &data, 
                                       const uint16_t &block_number){
  const std::vector<char> &frame_vector = frame->get_frame_as_vector();
  print_vector(frame_vector);
  ASSERT(frame_vector.size() >=4 , "Too small data frame");
  auto itr = frame_vector.cbegin();
  ASSERT(*itr == 0x00, "First byte of data frame is not 0x00");
  itr++;
  ASSERT(*itr == 0x03, "Invalid op code in data frame");
  ASSERT(frame->get_op_code() == tftp_frame::op_data, "Invalid op code in data frame object");
  itr++;
  ASSERT(frame->get_block_number() == block_number, "Invalid block number in data frame object");
  ASSERT(*itr == static_cast<char>(block_number >> 8), "Invalid block number in data frame");
  itr++;
  ASSERT(*itr == static_cast<char>(block_number), "Invalid block number in data frame");
  itr++;
  ASSERT(frame_vector.cend() >= itr, "Invalid data in data frame");
  auto max_data_len = tftp_frame::max_data_len;
  ASSERT(std::min(data.size(), max_data_len) == static_cast<std::size_t>(frame_vector.cend() - itr), "Invalid data in data frame");
  auto data_itr = data.cbegin();
  while(data_itr != data.cend() && itr != frame_vector.cend()){
    ASSERT(*data_itr == *itr, "Data mismatch in data frame");
    itr++;
    data_itr++;
  }
  ASSERT(itr == frame_vector.cend(), "Invalid data in data frame");
  return TEST_PASS;
}

TEST_RESULT test_data_request_frame_creation(){
  TEST_BEGIN;
  std::vector<uint16_t> block_number_sample;
  std::vector<std::vector<char>> data_sample;
  std::vector<uint16_t> data_length = {0, 1, 128, 512, 513, 1024};
  for(auto itr = data_length.cbegin(); itr != data_length.cend(); itr++) {
    std::vector<char> this_vector;
    for(uint16_t i = 0; i < *itr; i++){
      this_vector.push_back(random_number<int8_t>());
    }
    data_sample.push_back(this_vector);
    block_number_sample.push_back(random_number<uint16_t>());
  }

  tftp_frame_s data_frame;
  for(uint16_t i = 0; i < data_sample.size(); i++) {
    data_frame = tftp_frame::create_data_frame(data_sample[i].cbegin(), data_sample[i].cend(),
                                               block_number_sample[i]);
    CALL(validate_data_frame, data_frame, data_sample[i], block_number_sample[i]);
    std::stringstream ss;
    ss << "Data frame size " << data_sample[i].size();
    STATUS(ss.str());
  }
  TEST_END;
}
//test creation of ack request frame
//VC_TODO:

//test creation of error request frame
//VC_TODO:

//test creation of empty request frame
//VC_TODO:

//pass a bad frame with invalid op code
//VC_TODO:

//give a good read request frame and parse it
//VC_TODO:
//give a good write request frame and parse it
//VC_TODO:
//give a good data frame and parse it
//VC_TODO:
//give a good ack frame and parse it
//VC_TODO:
//give an invalid frame and parse it. Make a case for each kind of exception
//VC_TODO:

int main(){
  TEST_BEGIN;
  CALL(test_read_request_frame_creation);
  CALL(test_write_request_frame_creation);
  CALL(test_data_request_frame_creation);
  TEST_END;
}
