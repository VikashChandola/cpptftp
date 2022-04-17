#ifndef __FRAME_HPP__
#define __FRAME_HPP__

#include <algorithm>
#include <vector>
#include <string>

namespace tftp{
namespace packet{
constexpr int       delimiter_len   = 0x01;
constexpr uint8_t   delimiter       = 0x00;

constexpr int       block_number_len= 0x02;

enum class opcode : uint16_t {
    rrq     = 0x01,
    wrq     = 0x02,
    data    = 0x03,
    ack     = 0x04,
    err     = 0x05,
};

constexpr int opcode_len      = 0x02;

enum class error_code: uint16_t {
    not_defined = 0x00,
    file_not_found,
    access_violation,
    disk_allocation_error,
    illegal_operation,
    unknown_transfer_id,
    file_already_exist,
    no_such_user,
};

constexpr std::size_t error_code_len = 2;
constexpr std::size_t error_code_count = 8;

constexpr std::array<std::string_view, error_code_count> ec_desc = {
        "not_defined",
        "file_not_found",
        "access_violation",
        "disk_allocation_error",
        "illegal_operation",
        "unknown_transfer_id",
        "file_already_exist"
        "no_such_user"
};

namespace mode {
    constexpr std::string_view octet = "octet";
};

template <typename T>
std::pair<uint8_t, uint8_t> u8_pair(const T &t){
    static_assert(std::is_same<uint16_t, T>::value ||
                  std::is_same<opcode, T>::value ||
                  std::is_same<error_code, T>::value,
                  "Can't data into uint8_t pair");

    const uint16_t u16_val = static_cast<uint16_t>(t);
    return std::make_pair(static_cast<uint8_t>(u16_val >> 0x08) &0xFF,
                          static_cast<uint8_t>(u16_val & 0xFF));
}

struct base_packet {
    virtual ~base_packet() = default;
    virtual std::vector<uint8_t> buffer() const noexcept = 0;
};

struct rq_packet : public base_packet{
    std::string filename;
    rq_packet(const std::string &filename) : filename(filename){}
    std::vector<uint8_t> rq_buffer(const opcode &oc) const noexcept {
        std::vector<uint8_t> buf;
        auto oc_pair = u8_pair(oc);
        buf.push_back(oc_pair.first);
        buf.push_back(oc_pair.second);
        std::for_each(filename.cbegin(), filename.cend(),[&](const uint8_t& ch){
                    buf.push_back(ch);
                });
        buf.push_back(delimiter);
        std::for_each(mode::octet.cbegin(), mode::octet.cend(),[&](const uint8_t& ch){
                    buf.push_back(ch);
                });
        buf.push_back(delimiter);
        return buf;
    }
};

struct rrq_packet final : public rq_packet {
    rrq_packet(const std::string &filename) : rq_packet(filename){}
    std::vector<uint8_t> buffer() const noexcept override{
        return rq_buffer(opcode::rrq);
    }
};

struct wrq_packet final : public rq_packet {
    std::vector<uint8_t> buffer() const noexcept override{
        return rq_buffer(opcode::wrq);
    }
};

struct data_packet final: public base_packet{
    uint16_t block_number;
    std::vector<uint8_t> data;

    data_packet(const uint16_t &block_number, const std::vector<uint8_t> data):
        block_number(block_number), data(data){}

    std::vector<uint8_t> buffer() const noexcept override {
        std::vector<uint8_t> buf;
        auto oc_pair = u8_pair(opcode::data);
        buf.push_back(oc_pair.first);
        buf.push_back(oc_pair.second);
        auto block_number_pair = u8_pair(block_number);
        buf.push_back(block_number_pair.first);
        buf.push_back(block_number_pair.second);
        std::for_each(data.cbegin(), data.cend(),[&](const uint8_t &ch){
                    buf.push_back(ch);
                });
        return buf;
    }
};

struct ack_packet final : public base_packet {
    uint16_t block_number;

    ack_packet(const uint16_t &block_number): block_number(block_number){}
    std::vector<uint8_t> buffer() const noexcept override {
        std::vector<uint8_t> buf;
        auto oc_pair = u8_pair(opcode::ack);
        buf.push_back(oc_pair.first);
        buf.push_back(oc_pair.second);
        auto block_number_pair = u8_pair(block_number);
        buf.push_back(block_number_pair.first);
        buf.push_back(block_number_pair.second);
        return buf;
    }
};

struct err_packet final : public base_packet{
    error_code ec;
    err_packet(const error_code &ec): ec(ec){}
    std::vector<uint8_t> buffer() const noexcept override{
        std::vector<uint8_t> buf;
        auto oc_pair = u8_pair(opcode::err);
        auto ec_pair = u8_pair(ec);
        buf.push_back(oc_pair.first);
        buf.push_back(oc_pair.second);
        buf.push_back(ec_pair.first);
        buf.push_back(ec_pair.second);
        auto &err_msg = ec_desc[static_cast<uint16_t>(ec)];
        std::for_each(err_msg.cbegin(), err_msg.cend(),[&](const uint8_t &ch){
                    buf.push_back(ch);
                });
        buf.push_back(delimiter);
        return buf;
    }
};

} //namespace packet
} //namespace tftp
#endif
