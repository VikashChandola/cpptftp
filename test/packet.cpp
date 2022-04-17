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
                     bdata::make({"", "a", "filename", "file name with spaces"}),
                     filename_sample){
    const std::string filename(filename_sample);

    tftp::packet::rrq_packet packet = {filename};

    BOOST_TEST(packet.filename == filename);

    auto buffer = packet.buffer();
    BOOST_TEST(buffer.size() == tftp::packet::opcode_len +
                                filename.size() +
                                tftp::packet::delimiter_len +
                                tftp::packet::mode::octet.size() +
                                tftp::packet::delimiter_len);
    auto buffer_itr = buffer.cbegin();
    uint16_t extracted_opcode = (static_cast<uint16_t>(*buffer_itr) << 0x08) |
                                static_cast<uint16_t>(*(buffer_itr + 1));
    BOOST_TEST(extracted_opcode == static_cast<uint16_t>(tftp::packet::opcode::rrq));
    buffer_itr += tftp::packet::opcode_len;

    std::string extracted_filename = std::string(buffer_itr, buffer_itr + filename.size());
    BOOST_TEST(extracted_filename == filename);
    buffer_itr += filename.size();

    BOOST_TEST(*buffer_itr == tftp::packet::delimiter);
    buffer_itr += tftp::packet::delimiter_len;

    std::string extracted_mode = std::string(buffer_itr, buffer_itr + tftp::packet::mode::octet.size());
    BOOST_TEST(extracted_mode == tftp::packet::mode::octet);
    buffer_itr += tftp::packet::mode::octet.size();

    BOOST_TEST(*buffer_itr == tftp::packet::delimiter);
    buffer_itr += tftp::packet::delimiter_len;

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
    tftp::packet::data_packet packet = {block_number, data};
    BOOST_TEST(packet.block_number == block_number);
    BOOST_TEST(packet.data == data);

    auto buffer = packet.buffer();
    BOOST_TEST(buffer.size() == tftp::packet::opcode_len +
                                tftp::packet::block_number_len +
                                data.size());

    auto buffer_itr = buffer.cbegin();
    uint16_t extracted_opcode = (static_cast<uint16_t>(*buffer_itr) << 0x08) |
                                static_cast<uint16_t>(*(buffer_itr + 1));
    BOOST_TEST(extracted_opcode == static_cast<uint16_t>(tftp::packet::opcode::data));
    buffer_itr += tftp::packet::opcode_len;

    uint16_t extracted_block_number = (static_cast<uint16_t>(*buffer_itr) << 0x08) |
                                       static_cast<uint16_t>(*(buffer_itr + 1));
    BOOST_TEST(extracted_block_number == block_number);
    buffer_itr += tftp::packet::block_number_len;

    std::vector<uint8_t> extracted_data(buffer_itr, buffer_itr + data.size());
    BOOST_TEST(extracted_data == data);

    buffer_itr += data.size();
    BOOST_TEST((buffer_itr == buffer.cend()), "Invalid data packet buffer end");
}

BOOST_DATA_TEST_CASE(test_ack_packet_buffer_creation,
                     bdata::random((bdata::distribution=uint16_t_rand)) ^
                     bdata::xrange(5),
                     block_number_sample, index)
{
    (void)(index); //index is to limit test case count
    const uint16_t block_number = static_cast<uint16_t>(block_number_sample);
    tftp::packet::ack_packet packet = {block_number};
    BOOST_TEST(packet.block_number == block_number);

    auto buffer = packet.buffer();
    BOOST_TEST(buffer.size() == tftp::packet::opcode_len +
                                tftp::packet::block_number_len);

    auto buffer_itr = buffer.cbegin();
    uint16_t extracted_opcode = (static_cast<uint16_t>(*buffer_itr) << 0x08) |
                                static_cast<uint16_t>(*(buffer_itr + 1));
    BOOST_TEST(extracted_opcode == static_cast<uint16_t>(tftp::packet::opcode::ack));
    buffer_itr += tftp::packet::opcode_len;

    uint16_t extracted_block_number = (static_cast<uint16_t>(*buffer_itr) << 0x08) |
                                       static_cast<uint16_t>(*(buffer_itr + 1));
    BOOST_TEST(extracted_block_number == block_number);
    buffer_itr += tftp::packet::block_number_len;

    BOOST_TEST((buffer_itr == buffer.cend()), "Invalid ack packet buffer end");
}

BOOST_DATA_TEST_CASE(test_err_packet_buffer_creation,
                     bdata::xrange(tftp::packet::error_code_count),
                     error_code_sample){
    tftp::packet::error_code ec = static_cast<tftp::packet::error_code>(error_code_sample);
    tftp::packet::err_packet packet = {ec};
    const std::string err_msg(tftp::packet::ec_desc[error_code_sample].data());
    auto buffer = packet.buffer();
    BOOST_TEST(buffer.size() == tftp::packet::opcode_len +
                                tftp::packet::error_code_len +
                                err_msg.size() +
                                tftp::packet::delimiter_len);

    auto buffer_itr = buffer.cbegin();
    uint16_t extracted_opcode = (static_cast<uint16_t>(*buffer_itr) << 0x08) |
                                static_cast<uint16_t>(*(buffer_itr + 1));
    BOOST_TEST(extracted_opcode == static_cast<uint16_t>(tftp::packet::opcode::err));
    buffer_itr += tftp::packet::opcode_len;

    uint16_t extracted_ec = (static_cast<uint16_t>(*buffer_itr) << 0x08) |
                            static_cast<uint16_t>(*(buffer_itr + 1));
    BOOST_TEST((ec == static_cast<tftp::packet::error_code>(extracted_ec)), "error code mismatch");
    buffer_itr += tftp::packet::error_code_len;

    std::string extracted_err_msg = std::string(buffer_itr, buffer_itr + err_msg.size());
    buffer_itr += err_msg.size();

    BOOST_TEST(*buffer_itr == tftp::packet::delimiter);
    buffer_itr += tftp::packet::delimiter_len;

    BOOST_TEST((buffer_itr == buffer.cend()), "Invalid err packet buffer end");
}
