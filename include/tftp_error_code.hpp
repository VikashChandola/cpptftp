#ifndef __TFTP_ERROR_CODE_HPP__
#define __TFTP_ERROR_CODE_HPP__

namespace tftp {
typedef uint32_t error_code;
namespace error {
// Error code 1-100 are specific to tftp specification
const error_code no_error                = 0;
const error_code file_not_found          = 1;
const error_code access_violation        = 2;
const error_code disk_allocation_error   = 3;
const error_code illegal_tftp_operation  = 4;
const error_code unknown_transfer_id     = 5;
const error_code file_already_exists     = 6;
const error_code no_such_user            = 7;
//-------------------------------------------------------------------------------------------------
// Error code 101-999 are specific to application
const error_code connection_lost         = 101;
const error_code receive_timeout         = 102;
// Server responded with nonsense data
const error_code invalid_server_response = 103;
const error_code server_error_response   = 104;
// State machine reached an invalid state
const error_code state_machine_broke     = 106;
// Received too many packets from unknown endpoint
const error_code network_interference    = 107;
// Failed to write data to disk
const error_code disk_io_error           = 108;
const error_code user_requested_abort    = 109;

//-------------------------------------------------------------------------------------------------
// Boost Asio error codes are bubbled up to user by adding to this base
// For example if boost::asio give error code 7 then user will be called back with boost_asio_error_base + 7
// This range is reserved upto boost_asio_error_base + 999
const error_code boost_asio_error_base   = 1000;

//-------------------------------------------------------------------------------------------------
} // namespace error
} // namespace tftp

#endif
