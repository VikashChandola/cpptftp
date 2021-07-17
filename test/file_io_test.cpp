#define BOOST_TEST_MODULE file io test

#include "file_io.hpp"
#include <boost/test/data/monomorphic.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/included/unit_test.hpp>
#include <random>
#include <stdexcept>

// Anyhting more than 1MB is too large
#define MAX_BUFFER_SIZE 1048576
class reader_fixture {
  std::string filename;
  uint64_t block_size;
  uint64_t block_count;
  char *data_ptr;

public:
  reader_fixture(std::string filename, uint64_t block_size, uint64_t block_count)
      : filename(filename),
        block_size(block_size),
        block_count(block_count),
        data_ptr(nullptr) {
    std::stringstream ss;
    // PLATFORM_NOTE
    ss << "dd status=none bs=" << this->block_size << " count=" << this->block_count
       << " if=/dev/urandom of=" << this->filename << " > /dev/null 2>&1";

    // ss.str("");
    // ss << "echo 1234567890123456 > " << this->filename;
    std::system(ss.str().c_str());
  }

  std::unique_ptr<char[]> get() {
    uint64_t file_size = this->block_size * this->block_count;
    std::unique_ptr<char[]> data_buffer(new char[file_size]);
    if (file_size > MAX_BUFFER_SIZE) {
      throw std::runtime_error("Too large file to read");
    }
    std::fstream handle(this->filename, std::ios::in | std::ios::binary);
    handle.read(data_buffer.get(), file_size);
    return data_buffer;
  }

  ~reader_fixture() {
    std::stringstream ss;
    // PLATFORM_NOTE
    ss << "rm -rf " << this->filename;
    std::system(ss.str().c_str());
    if (this->data_ptr) {
      delete[] this->data_ptr;
    }
  }
};

int get_random_number(int lower, int upper) {
  std::random_device dev;
  std::mt19937 rng(dev());
  std::uniform_int_distribution<std::mt19937::result_type> dist(lower, upper);
  return dist(rng);
}

namespace bdata = boost::unit_test::data;
BOOST_DATA_TEST_CASE(file_io_general_test,
                     bdata::make({512, 1024, 1023, 2040}) ^ bdata::make(1, 20, 31, 12),
                     block_size,
                     block_count) {
  std::stringstream ss;
  ss << block_size << "_" << block_count;
  std::string filename = ss.str();
  auto r_fixture       = reader_fixture(filename, block_size, block_count);
  auto data_ptr        = r_fixture.get();
  fileio::reader sample_reader(filename);
  BOOST_TEST(sample_reader.is_open(), "Failed to open file");
  std::vector<char> sample_data;
  std::streamsize bytes_read, read_size, total_read = 0;
  size_t index = 0;

  read_size    = get_random_number(1, block_size * 2);
  sample_data.resize(read_size);
  while (sample_reader.fill_buffer(sample_data.begin(), sample_data.end(), bytes_read)) {
    auto count = std::min(sample_data.end() - sample_data.begin(), bytes_read);
    std::stringstream ss;
    ss << "Wanted " << read_size << ", Read :" << bytes_read;
    BOOST_TEST_MESSAGE(ss.str());
    total_read += bytes_read;
    if (std::equal(sample_data.begin(), sample_data.begin() + count, data_ptr.get() + index)) {
      index += bytes_read;
    } else {
      BOOST_TEST(false, "Data mismatch");
      break;
    }
    if (bytes_read != read_size) {
      break;
    }
    read_size = get_random_number(1, block_size * 2);
    sample_data.resize(read_size);
  }
  BOOST_TEST(total_read == block_size * block_count, "Number of bytes read is not as much as expected");
}

BOOST_AUTO_TEST_CASE(file_io_read_non_existing_file) {
  std::string non_existing_file = "file_io_test_random_file";
  {
    // This basically makes sure that this file gets delted if exists. It creates and "deletes" a file
    auto r_fixture = reader_fixture(non_existing_file, 1, 1);
    (void)(r_fixture);
  }
  fileio::reader sample_reader(non_existing_file);
  BOOST_TEST(sample_reader.is_open() == false, "Non existing file is said as open");
  std::array<char, 16> buffer;
  std::streamsize bytes_read = 0;
  BOOST_TEST(sample_reader.fill_buffer(buffer.begin(), buffer.end(), bytes_read) == false,
             "Buffer was filled for non reader associated with non existing file");
  BOOST_TEST(bytes_read == 0, "Numer of bytes read is non zero");
}

BOOST_DATA_TEST_CASE(file_io_writer_test,
                     (bdata::make({513, 512}) ^ bdata::make({10, 8})) * bdata::make({true, false}),
                     block_size,
                     block_count,
                     lazy) {
  const std::string filename("file_io_writer_test");
  fileio::writer sample_writer(filename, lazy);
  if (lazy) {
    BOOST_TEST(!sample_writer.is_open(), "fileio::writer opened file for writting");
  } else {
    BOOST_TEST(sample_writer.is_open(), "fileio::writer have not opened file for writting");
  }
  std::vector<char> data_write;
  for (int i = 0; i < block_size; i++) {
    data_write.push_back(static_cast<char>(i % 128));
  }
  for (int i = 0; i < block_count; i++) {
    sample_writer.write_buffer(data_write.cbegin(), data_write.cend());
    BOOST_TEST(sample_writer.is_open(), "fileio::writer didn't open file");
  }
  std::fstream read_handle(filename, std::ios::in | std::ios::binary);
  std::vector<char> data_read;
  data_read.resize(block_size);
  int bytes_read = 0;
  for (int i = 0; i < block_count; i++) {
    read_handle.read(data_read.data(), block_size);
    BOOST_TEST(std::equal(data_read.cbegin(), data_read.cend(), data_write.cbegin()));
    bytes_read += (data_read.cend() - data_read.cbegin());
  }
  BOOST_TEST(bytes_read == block_size * block_count, "Mismatch in number of bytes read and written");
  std::stringstream ss;
  ss << "rm -rf " << filename;
  std::system(ss.str().c_str());
}
