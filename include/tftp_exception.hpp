#ifndef __TFTP_EXCEPTION_H__
#define __TFTP_EXCEPTION_H__

#include <exception>
#include <string>
namespace tftp {

// Base exception class
class exception : public std::exception {
public:
  exception(const std::string &err_message) : std::exception(), message(err_message) {}
  virtual const char *what() const throw() { return this->message.c_str(); }

protected:
  const std::string message;
};

// Given feature is not yet implemented
class missing_feature_exception : public exception {
  using exception::exception;
};

// Invalid frame, can't be parsed
class framing_exception : public exception {
  using exception::exception;
};

// An invalid parameter is encountered for example op code with value 0x12, or requested parameter is not part
// of given frame here
class invalid_frame_parameter_exception : public framing_exception {
  using framing_exception::framing_exception;
};

// Frame is not complete. Either it's a broken frame or more is yet to come from remote end
class partial_frame_exception : public framing_exception {
  using framing_exception::framing_exception;
};

// Frame is not complete. Either it's a broken frame or more is yet to come from remote end
class frame_type_mismatch_exception : public framing_exception {
  using framing_exception::framing_exception;
};

} // namespace tftp
#endif //__TFTP_EXCEPTION_H__
