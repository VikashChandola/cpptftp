#ifndef __FILE_IO_HPP__
#define __FILE_IO_HPP__
#include <fstream>

namespace fileio {

class reader {
  std::fstream handle;
  std::unique_ptr<char[]> buffer;
public:
  base(const std::string &filename, uint16_t buffer_size = 512)
    : handle(filename, std::ios::in | std::ios::binary),
      buffer(new char[buffer_size])
  {
  }

  bool is_open(){
    return this->handle.is_open();
  }

  /* Reads from disk and start filling from itr_begin till itr_end. If end of file hit before
   * reaching itr_end then is_full will be set to false, otherwise is_full will be set to true.
   * Returns true if read operation was successfull otherwise return false
   */
  template<typename T>
  bool fill_buffer(T itr_begin, T itr_end, bool &is_full ) noexcept {
    if(!this->is_open()){
      return false;
    }
    try{
      this->read_stream.read(this->buffer.get(), itr_end - itr_begin);
    } catch (const std::ios_base::failure& e){
      this->handle.close();
      return false;
    }
    while(itr_begin != itr_end){
      *itr_begin = 
    }
  }
protected:
};

class writer {};
} // namespace fileio

#endif
