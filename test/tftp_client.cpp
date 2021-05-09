#define BOOST_TEST_MODULE tftp client download only
#include <boost/test/data/test_case.hpp>
#include <boost/test/included/unit_test.hpp>
#include <exception>

#include "tftp_client.hpp"
#include "tftp_exception.hpp"

namespace bdata = boost::unit_test::data;

class tftp_client_fixture {
public:
  tftp_client_fixture() : callback_invoked(false) {
    BOOST_TEST_REQUIRE(boost::unit_test::framework::master_test_suite().argc == 3);
    ip = boost::unit_test::framework::master_test_suite().argv[1];
    port = std::stoi(boost::unit_test::framework::master_test_suite().argv[2]);
    work_dir = "work_dir";
    std::stringstream ss;
    ss << "rm -rf " << work_dir << "/*";
    std::system(ss.str().c_str());
    ss.str("");
    ss << "mkdir -p " << work_dir;
    std::system(ss.str().c_str());
  }

  void setup() {}

  ~tftp_client_fixture() {
    std::stringstream ss;
    ss << "rm -rf ";
    ss << work_dir;
    ss << "/*";
    ss << " > /dev/null 2>&1 ";
    std::system(ss.str().c_str());
  }

  void teardown() {}

  uint16_t get_port() { return this->port; }

  std::string get_ip() { return this->ip; }

  bool add_file(std::string filename, uint32_t block_size, uint32_t block_count) {
    std::stringstream ss;
    ss << "dd bs=";
    ss << block_size;
    ss << " count=";
    ss << block_count;
    ss << " if=/dev/urandom  of=";
    ss << this->work_dir + "/" + filename;
    ss << " >  /dev/null 2>&1";
    if (std::system(ss.str().c_str())) {
      return false;
    }
    return true;
  }

  bool remove_file(std::string filename) {
    std::stringstream ss;
    ss << "rm -rf ";
    ss << this->work_dir + "/" + filename;
    if (std::system(ss.str().c_str())) {
      return false;
    }
    return true;
  }

  void completion_callback(tftp::error_code e, std::string downloaded_filename) {
    BOOST_TEST_MESSAGE("ret code :" << e);
    callback_invoked = true;
    BOOST_REQUIRE_MESSAGE((e == 0), "Download failed with error code :" << e);
    std::string remote_filename = this->work_dir + "/" + downloaded_filename;

    std::ifstream ifs1(remote_filename);
    std::ifstream ifs2(downloaded_filename);
    std::istream_iterator<char> b1(ifs1), e1;
    std::istream_iterator<char> b2(ifs2), e2;
    BOOST_CHECK_EQUAL_COLLECTIONS(b1, e1, b2, e2);
    std::stringstream ss;
    ss << "rm -f ";
    ss << downloaded_filename;
    std::system(ss.str().c_str());
  }

  bool is_callback_invoked() { return callback_invoked; }

private:
  std::string work_dir;
  std::string ip;
  uint16_t port;
  bool callback_invoked;
};

BOOST_DATA_TEST_CASE_F(tftp_client_fixture, download_existing_file_of_known_size,
                       bdata::make({1, 1, 512, 513, 513, 513, 8192}) ^ bdata::make({0, 1, 1, 1, 2, 3, 4095}),
                       block_size, count) {
  std::string filename = std::to_string(block_size) + "_" + std::to_string(count);
  BOOST_TEST_MESSAGE("Creating file...");
  add_file(filename, block_size, count);

  boost::asio::io_context io;
  std::string ip = get_ip();
  uint16_t port = get_port();
  udp::resolver resolver(io);
  udp::endpoint remote_endpoint;
  BOOST_TEST_MESSAGE("Resolving endpoing...");
  try {
    remote_endpoint = *resolver.resolve(udp::v4(), ip, std::to_string(port)).begin();
  } catch (...) {
    BOOST_TEST_MESSAGE("Endpoint Resolution failure.");
    BOOST_REQUIRE(false);
  }

  BOOST_TEST_MESSAGE("Creating client...");
  tftp::client_s tftp_client = tftp::client::create(io, remote_endpoint);
  BOOST_TEST_MESSAGE("Downloading file...");
  tftp_client->download_file(filename, filename, [&](tftp::error_code e) { completion_callback(e, filename); });
  io.run();
  BOOST_TEST((is_callback_invoked() == true), "download_file method didn't invoke callback");
}
