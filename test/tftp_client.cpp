#define BOOST_TEST_MODULE tftp client download only
#include <boost/test/data/test_case.hpp>
#include <boost/test/included/unit_test.hpp>
#include <exception>

#include "tftp_client.hpp"
#include "tftp_exception.hpp"

namespace bdata = boost::unit_test::data;

class tftp_server_fixture {
public:
  tftp_server_fixture() {
    BOOST_TEST_REQUIRE(boost::unit_test::framework::master_test_suite().argc == 3);
    ip = boost::unit_test::framework::master_test_suite().argv[1];
    port = std::stoi(boost::unit_test::framework::master_test_suite().argv[2]);
    work_dir = "work_dir";
    std::stringstream ss;
    ss << "rm -rf " << work_dir;
    std::system(ss.str().c_str());
    ss.str("");
    ss << "mkdir -p " << work_dir;
    std::system(ss.str().c_str());
  }

  void setup() {}

  ~tftp_server_fixture() {
    std::stringstream ss;
    ss << "rm -rf ";
    ss << work_dir;
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
    ss << " < /dev/urandom  > ";
    ss << this->work_dir + "/" + filename;
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

private:
  void run_server() {
    std::stringstream ss;
    ss << "./bin/pyTFTP/server.py ";
    ss << work_dir;
    ss << " -H ";
    ss << ip;
    ss << " -p ";
    ss << std::to_string(port);

    std::system(ss.str().c_str());
  }

  std::string work_dir;
  std::string ip;
  uint16_t port;
};

BOOST_DATA_TEST_CASE_F(tftp_server_fixture, download_existing_file_of_known_size,
                       bdata::make({0, 100, 512, 513, 513, 513, 512}) ^ bdata::make({1, 1, 1, 1, 2, 4, 65535}),
                       block_size, count) {
  add_file(std::to_string(block_size) + "_" + std::to_string(count), block_size, count);
}
