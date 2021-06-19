#ifndef __TFTP_ERROR_CODE_HPP__
#define __TFTP_ERROR_CODE_HPP__

namespace tftp {
typedef uint32_t error_code;

const error_code no_error                = 0;
const error_code file_not_found          = 1;
const error_code access_violation        = 2;
const error_code disk_allocation_error   = 3;
const error_code illegal_tftp_operation  = 4;
const error_code unknown_transfer_id     = 5;
const error_code file_already_exists     = 6;
const error_code no_such_user            = 7;

const error_code connection_lost         = 101;
const error_code receive_timeout         = 102;
const error_code invalid_server_response = 103;
const error_code server_error_response   = 104;

} // namespace tftp

#endif
