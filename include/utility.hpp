#ifndef __UTILITY_HPP__
#define __UTILITY_HPP__
//Set of dead simple functions and other things that can't find it's in any other header

template <typename T>
std::string to_string(const T &endpoint) {
  std::stringstream ss;
  ss << endpoint;
  return ss.str();
}
#endif
