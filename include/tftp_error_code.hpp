#ifndef __TFTP_ERROR_CODE_HPP__
#define __TFTP_ERROR_CODE_HPP__

namespace tftp {
typedef uint32_t error_code;

const error_code no_error                 = 0;
const error_code connection_lost          = 1;
const error_code receive_timeout          = 2;
const error_code invalid_server_response  = 3;
const error_code server_error_response    = 4;

} // namespace tftp

#endif
