#ifndef __FILE_IO_HPP__
#define __FILE_IO_HPP__
#include <fstream>
#include <memory>

namespace fileio {

class reader {
  std::fstream handle;

public:
  reader(const std::string &filename) : handle(filename, std::ios::in | std::ios::binary) {}

  bool is_open() { return this->handle.is_open(); }

  /* Reads from disk and start filling from itr_begin till itr_end. Returns true if read went through without
   * any problem. bytes_read argument is set to number of bytes read from the file
   */
  template <typename T>
  bool fill_buffer(T itr_begin, T itr_end, std::streamsize &bytes_read) noexcept {
    size_t buffer_size = itr_end - itr_begin;
    std::unique_ptr<char[]> buffer(new char[buffer_size]);
    if (!this->is_open()) {
      return false;
    }
    try {
      this->handle.read(buffer.get(), buffer_size);
      bytes_read = this->handle.gcount();
    } catch (const std::ios_base::failure &e) {
      this->handle.close();
      return false;
    }
    std::copy(&buffer.get()[0], &buffer.get()[0] + buffer_size, itr_begin);
    return true;
  }

protected:
};

class writer {};
} // namespace fileio

#endif
