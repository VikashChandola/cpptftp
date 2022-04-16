#ifndef __FRAME_HPP__
#define __FRAME_HPP__

#include <algorithm>
#include <boost/asio/buffer.hpp>
namespace tftp{

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
    buf.push_back(0x00);
    std::for_each(packet.filename.cbegin(), packet.filename.cend(),[&](auto ch){
                buf.push_back(static_cast<uint8_t>(ch));
            });
    std::for_each(mode::octet.cbegin(), mode::octet.cend(),[&](auto ch){
                buf.push_back(static_cast<uint8_t>(ch));
            });
    buf.push_back(0x00);
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
