#ifndef __FRAME_HPP__
#define __FRAME_HPP__

#include <algorithm>
#include <vector>
#include <string>

namespace tftp{
namespace packet_const {

constexpr int       delimiter_len   = 0x01;
constexpr uint8_t   delimiter       = 0x00;

constexpr int       block_number_len= 0x02;

constexpr int       opcode_len      = 0x02;
constexpr uint16_t  rrq_opcode      = 0x01;
constexpr uint16_t  wrq_opcode      = 0x02;
constexpr uint16_t  data_opcode     = 0x03;
constexpr uint16_t  ack_opcode      = 0x04;
constexpr uint16_t  err_opcode      = 0x05;

}

namespace mode {
    constexpr std::string_view octet = "OCTET";
};

struct rq_packet{
    uint16_t opcode;
    std::string filename;
};

typedef rq_packet rrq_packet;
typedef rq_packet wrq_packet;

std::vector<uint8_t> CreateBuffer(const rq_packet &packet){
    std::vector<uint8_t> buf;
    buf.push_back(static_cast<uint8_t>((packet.opcode >> 8 ) & 0xFF));
    buf.push_back(static_cast<uint8_t>(packet.opcode & 0xFF));
    std::for_each(packet.filename.cbegin(), packet.filename.cend(),[&](auto ch){
                buf.push_back(static_cast<uint8_t>(ch));
            });
    buf.push_back(packet_const::delimiter);
    std::for_each(mode::octet.cbegin(), mode::octet.cend(),[&](auto ch){
                buf.push_back(static_cast<uint8_t>(ch));
            });
    buf.push_back(packet_const::delimiter);
    return buf;
}

struct data_packet{
    uint16_t opcode;
    uint16_t block_number;
    std::vector<uint8_t> data;
};

std::vector<uint8_t> CreateBuffer(const data_packet &packet){
    std::vector<uint8_t> buf;
    buf.push_back(static_cast<uint8_t>((packet.opcode >> 8 ) && 0xFF));
    buf.push_back(static_cast<uint8_t>(packet.opcode & 0xFF));
    buf.push_back(static_cast<uint8_t>((packet.block_number >> 8 ) & 0xFF));
    buf.push_back(static_cast<uint8_t>(packet.block_number & 0xFF));
    std::for_each(packet.data.cbegin(), packet.data.cend(),[&](auto ch){
                buf.push_back(static_cast<uint8_t>(ch));
            });
    return buf;
}

struct ack_packet{
    uint16_t opcode;
    uint16_t block_number;
};

std::vector<uint8_t> CreateBuffer(const ack_packet &packet){
    std::vector<uint8_t> buf;
    buf.push_back(static_cast<uint8_t>((packet.opcode >> 8 ) & 0xFF));
    buf.push_back(static_cast<uint8_t>(packet.opcode & 0xFF));
    buf.push_back(static_cast<uint8_t>((packet.block_number >> 8 ) & 0xFF));
    buf.push_back(static_cast<uint8_t>(packet.block_number & 0xFF));
    return buf;
}

struct err_packet{
    uint16_t opcode;
    uint16_t error_code;
    std::string err_msg;
};

std::vector<uint8_t> CreateBuffer(const err_packet &packet){
    std::vector<uint8_t> buf;
    buf.push_back(static_cast<uint8_t>((packet.opcode >> 8 ) & 0xFF));
    buf.push_back(static_cast<uint8_t>(packet.opcode & 0xFF));
    buf.push_back(static_cast<uint8_t>((packet.error_code >> 8 ) & 0xFF));
    buf.push_back(static_cast<uint8_t>(packet.error_code & 0xFF));
    std::for_each(packet.err_msg.cbegin(), packet.err_msg.cend(),[&](auto ch){
                buf.push_back(static_cast<uint8_t>(ch));
            });
    return buf;
}
} //namespace tftp
#endif
