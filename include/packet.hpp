#ifndef __FRAME_HPP__
#define __FRAME_HPP__

#include <algorithm>
#include <vector>
#include <string>
#include <algorithm>
#include <iterator>
/* packet provides ability to manipulate data packets for tftp transactions
 *
 *      +-----------+       +---------------+       +-----------+
 *      | data_1    |       |               |       | header    |
 *      | data_2    |------>| Conversion    |------>|-----------+
 *      | ...       |<------| engine        |<------| body      |
 *      |           |       |               |       |..         |
 *      +-----------+       +---------------+       +-----------+
 *      Structured                                  Raw packet for
 *      Data                                        Network
 * []_packet structures denote a network packet. A network packet can be seen as in two form
 * structured form where we can get individual entries of the packet or network form where data
 * is suitable for transaction but not easy to use from application usage point of view.
 * []_packet structues allow creation(construction) of packet either via structured information
 * or directly from raw packet. This provides an implicit coversion engine between two
 * representation of a packet.
 * Class heirarchy
 * base_packet
 *      |
 *      |____________________________________________
 *      |           |               |               |
 *  rq_packet   data_packet     ack_packet      err_packet
 *      |___________
 *      |           |
 *  rrq_packet  wrq_packet
 *
 *  Each tail child object represent one kind of packet as per RFC1350
 *  All packet expose following APIs
 *  1.  Constructor to create packet object from minimal required data. For example to create a
 *      ack_packet user need to provide block_number.
 *  2.  Constructor to create packet object from raw network buffer.
 *  3.  method to create raw network buffer for packet for packet object.
 *  Here '1' and '3' gives ability to go from left to right and '2' allows conversion from right
 *  to left.
 */

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

constexpr std::size_t opcode_len      = 0x02;
constexpr std::size_t opcode_count    = 0x05;

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
        "file_already_exist",
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

uint16_t get_u16(const std::vector<uint8_t>::const_iterator &it){
    return ((static_cast<uint16_t>(*it) << 0x08) | static_cast<uint16_t>(*(it+1)));
}

/* gives error code as for given err_packet packet
 */
error_code get_error_code(const std::vector<uint8_t> &buf){
    assert(buf.size() > opcode_len + error_code_len);
    uint16_t ec_u16 = get_u16(buf.cbegin() + opcode_len);
    assert(ec_u16 < error_code_count);
    return static_cast<error_code>(ec_u16);
}

/* gives opcode as for given packet
 */
opcode get_opcode(const std::vector<uint8_t> &buf){
    assert(buf.size() > opcode_len);
    uint16_t oc_u16 = get_u16(buf.cbegin());
    assert((oc_u16 > 0) && (oc_u16 < (opcode_count + 1)));
    return static_cast<opcode>(oc_u16);
}

/* gives error message for given err_packet packet
 */
std::string get_err_message(const std::vector<uint8_t> &buf){
    assert(get_opcode(buf) == opcode::err);
    assert(buf.size() >= opcode_len + block_number_len + delimiter_len);
    auto delim_itr = std::ranges::find(buf.cbegin() + opcode_len + block_number_len,
                                       buf.cend(), delimiter);
    return std::string(buf.cbegin() + opcode_len + block_number_len, delim_itr);
}

/* Returns true if buffer given in first argument is of type opcode that second argument
 */
bool is_same(const std::vector<uint8_t> &buf, const opcode &oc){
    opcode buffer_oc = get_opcode(buf);
    return (buffer_oc == oc);
}

struct base_packet {
    virtual ~base_packet() = default;
    virtual std::vector<uint8_t> buffer() const noexcept = 0;
};

struct rq_packet : public base_packet{
    std::string filename;

    rq_packet(const std::string &filename) : filename(filename){}

    rq_packet(const std::vector<uint8_t> &buf) {
        assert(get_opcode(buf) == opcode::rrq);
        auto filename_delim_itr = std::ranges::find(buf.cbegin() + opcode_len, buf.cend(),
                                                    delimiter);
        assert(filename_delim_itr != buf.cend());
        filename = std::string(buf.cbegin() + opcode_len, filename_delim_itr);
        auto mode_delim_itr = std::ranges::find(filename_delim_itr + 1, buf.cend(), delimiter);
        assert(mode_delim_itr != buf.cend());
        assert(std::string(filename_delim_itr + 1, mode_delim_itr) == mode::octet);
        assert((mode_delim_itr + 1) == buf.cend());
    }

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
    rrq_packet(const std::vector<uint8_t> &buf): rq_packet(buf){}
    std::vector<uint8_t> buffer() const noexcept override{
        return rq_buffer(opcode::rrq);
    }
};

struct wrq_packet final : public rq_packet {
    wrq_packet(const std::string &filename) : rq_packet(filename){}
    wrq_packet(const std::vector<uint8_t> &buf): rq_packet(buf){}
    std::vector<uint8_t> buffer() const noexcept override{
        return rq_buffer(opcode::wrq);
    }
};

struct data_packet final: public base_packet{
    uint16_t block_number;
    std::vector<uint8_t> data;

    data_packet(const uint16_t &block_number, const std::vector<uint8_t> data):
        block_number(block_number), data(data){}

    data_packet(const std::vector<uint8_t> &buf) {
        assert(get_opcode(buf) == opcode::data);
        assert(buf.size() >= opcode_len + block_number_len);
        block_number = get_u16(buf.cbegin() + opcode_len);
        std::for_each(buf.cbegin() + opcode_len+ block_number_len,
                      buf.cend(),
                      [&](const uint8_t &ch){
                            data.push_back(ch);
                      });
    }
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

    ack_packet(const std::vector<uint8_t> &buf) {
        assert(get_opcode(buf) == opcode::ack);
        assert(buf.size() == opcode_len + block_number_len);
        block_number = get_u16(buf.cbegin() + opcode_len);
    }

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

    err_packet(const std::vector<uint8_t> &buf) {
        assert(get_opcode(buf) == opcode::err);
        assert(buf.size() > opcode_len + block_number_len + delimiter_len);
        ec = get_error_code(buf);
        auto delim_itr = std::ranges::find(buf.cbegin() + opcode_len + block_number_len,
                                           buf.cend(), delimiter);
        assert(delim_itr < buf.cend());
        assert(delim_itr + 1 == buf.cend());
    }

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
