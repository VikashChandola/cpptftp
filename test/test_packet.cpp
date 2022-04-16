#define BOOST_TEST_MODULE tftp_packet

//#include "packet.hpp"
#include <boost/test/included/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/included/unit_test.hpp>
#include <limits>

#include "packet.hpp"

namespace bdata = boost::unit_test::data;

auto uint16_t_rand = std::uniform_int_distribution<uint16_t>(0, std::numeric_limits<uint16_t>::max());
auto uint8_t_rand = std::uniform_int_distribution<uint8_t>(0, std::numeric_limits<uint8_t>::max());

BOOST_DATA_TEST_CASE(test_request_packet_buffer_creation,
                     bdata::random((bdata::distribution = uint16_t_rand)) ^
                     bdata::make({"", "a", "filename", "file name with spaces"}),
                     opcode_sample, filename_sample){
    const std::string filename(filename_sample);
    const uint16_t opcode = static_cast<uint16_t>(opcode_sample);
    tftp::rq_packet packet = {opcode, filename};
    BOOST_TEST(packet.opcode == opcode);
    BOOST_TEST(packet.filename == filename);

    auto buffer = CreateBuffer(packet);
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

BOOST_DATA_TEST_CASE(test_data_packet_buffer_creation,
                     bdata::random((bdata::distribution=uint16_t_rand)) ^
                     bdata::make({0, 255, 512}),
                     block_number_sample,
                     data_len_sample)
{
    const uint16_t block_number = static_cast<uint16_t>(block_number_sample);
    std::vector<uint8_t> data;
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        for(int i = 0; i < data_len_sample; i++){
            data.push_back(uint8_t_rand(gen));
        }
    }
    tftp::data_packet packet = {tftp::packet_const::data_opcode, block_number, data};
    BOOST_TEST(packet.opcode == tftp::packet_const::data_opcode);
    BOOST_TEST(packet.block_number == block_number);
    BOOST_TEST(packet.data == data);

    auto buffer = CreateBuffer(packet);
    BOOST_TEST(buffer.size() == tftp::packet_const::opcode_len +
                                tftp::packet_const::block_number_len +
                                data.size());

    auto buffer_itr = buffer.cbegin();
    uint16_t extracted_opcode = (static_cast<uint16_t>(*buffer_itr) << 0x08) |
                                static_cast<uint16_t>(*(buffer_itr + 1));
    BOOST_TEST(extracted_opcode == tftp::packet_const::data_opcode);
    buffer_itr += tftp::packet_const::opcode_len;

    uint16_t extracted_block_number = (static_cast<uint16_t>(*buffer_itr) << 0x08) |
                                       static_cast<uint16_t>(*(buffer_itr + 1));
    BOOST_TEST(extracted_block_number == block_number);
    buffer_itr += tftp::packet_const::block_number_len;

    std::vector<uint8_t> extracted_data(buffer_itr, buffer_itr + data.size());
    BOOST_TEST(extracted_data == data);

    buffer_itr += data.size();
    BOOST_TEST((buffer_itr == buffer.cend()), "Invalid data packet buffer end");
}

/*
BOOST_DATA_TEST_CASE(test_ack_packet_buffer_creation,
                     bdata::random((bdata::distribution=uint16_t_rand)) ^
                     block_number_sample)
{
    const uint16_t block_number = static_cast<uint16_t>(block_number_sample);
}*/
