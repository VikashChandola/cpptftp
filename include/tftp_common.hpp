#ifndef __TFTP_COMMON_HPP__
#define __TFTP_COMMON_HPP__

template <typename T>
std::string to_string(const T &endpoint) {
  std::stringstream ss;
  ss << endpoint;
  return ss.str();
}
#endif
