#ifndef __TFTP_COMMON_HPP__
#define __TFTP_COMMON_HPP__

#include <boost/asio.hpp>

static const boost::asio::chrono::duration<uint64_t, std::milli> default_network_timeout(1000);
static const uint16_t default_max_retry_count(3);

#endif
