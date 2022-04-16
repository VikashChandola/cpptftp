#define BOOST_TEST_MODULE tftp_packet

//#include "packet.hpp"
#include <boost/test/included/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/included/unit_test.hpp>

#include "packet.hpp"

namespace bdata = boost::unit_test::data;

BOOST_DATA_TEST_CASE(test_request_packet_buffer_creation,
                     bdata::random((bdata::distribution=std::uniform_int_distribution<uint16_t>(0, 255)))
                     ^ bdata::make({"", "a", "filename", "file name with spaces"}),
                     opcode, filename_cstr){
    const std::string filename(filename_cstr);
    tftp::rq_packet packet = {opcode, filename};
    BOOST_TEST(packet.opcode == opcode);
    BOOST_TEST(packet.filename == filename);

    std::vector buffer = CreateBuffer(packet);
    BOOST_TEST(buffer.size() == tftp::packet_const::opcode_len +
                                filename.size() +
                                tftp::packet_const::delimiter_len +
                                tftp::mode::octet.size() +
                                tftp::packet_const::delimiter_len);
    auto buffer_itr = buffer.cbegin();
    uint16_t extracted_opcode = (static_cast<uint16_t>(*buffer_itr) << 0x08) |
                                static_cast<uint16_t>(*(buffer_itr + 1));
    BOOST_TEST(extracted_opcode == opcode);
    buffer_itr += tftp::packet_const::opcode_len;

    std::string extracted_filename = std::string(buffer_itr, buffer_itr + filename.size());
    BOOST_TEST(extracted_filename == filename);
    buffer_itr += filename.size();

    BOOST_TEST(*buffer_itr == tftp::packet_const::delimiter);
    buffer_itr += tftp::packet_const::delimiter_len;

    std::string extracted_mode = std::string(buffer_itr, buffer_itr + tftp::mode::octet.size());
    BOOST_TEST(extracted_mode == tftp::mode::octet);
    buffer_itr += tftp::mode::octet.size();

    BOOST_TEST(*buffer_itr == tftp::packet_const::delimiter);
    buffer_itr += tftp::packet_const::delimiter_len;

    BOOST_TEST((buffer_itr == buffer.cend()));
}
