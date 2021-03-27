#ifndef __TFTP_EXCEPTION_H__
#define __TFTP_EXCEPTION_H__

#include <exception>
#include <string>

// Base exception class
class tftp_exception : public std::exception {
public:
  tftp_exception (const std::string &err_message) : std::exception(), message(err_message) {
  }
  virtual const char* what() const throw() {
    return this->message.c_str();
  }
protected:
  const std::string message;
};

// Given feature is not yet implemented
class tftp_missing_feature_exception : public tftp_exception {
  using tftp_exception::tftp_exception;
};

// Invalid frame, can't be parsed
class tftp_framing_exception : public tftp_exception {
  using tftp_exception::tftp_exception;
};

//An invalid parameter is encountered for example op code with value 0x12, or requested parameter is not part of
//given frame here
class tftp_invalid_frame_parameter_exception : public tftp_framing_exception {
  using tftp_framing_exception::tftp_framing_exception;
};

//Frame is not complete. Either it's a broken frame or more is yet to come from remote end
class tftp_partial_frame_exception : public tftp_framing_exception {
  using tftp_framing_exception::tftp_framing_exception;
};

#endif //__TFTP_EXCEPTION_H__
