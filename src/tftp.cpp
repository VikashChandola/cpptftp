#include <iostream>
#include <vector>
#include <string>
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <variant>
#include <fstream>

#include "packet.hpp"

namespace tftp {

enum class Status: int {
    success = 0,
    unknownError,
    networkFailure,
    retryThrottle
};

using boost::asio::ip::udp;

constexpr size_t BLOCK_SIZE = 512;
udp::endpoint any_endpoint;


Status sendReadRequest(boost::asio::yield_context &yield,
                       udp::socket &socket,
                       const udp::endpoint &remote_endpoint,
                       const std::string &filename){
    packet::rrq_packet packet = {filename};
    auto buffer = packet.buffer();
    boost::system::error_code ec;
    std::cout << remote_endpoint << std::endl;
    std::size_t txSize = socket.async_send_to(
            boost::asio::mutable_buffer(buffer.data(), buffer.size()),
            remote_endpoint,
            yield[ec]);
    if(ec || txSize != buffer.size()){
        std::cerr << ec << std::endl;
        return Status::networkFailure;
    }
    return Status::success;
}

Status receivePacket(boost::asio::yield_context &yield,
                     boost::asio::io_context &,
                     udp::socket &socket,
                     boost::asio::mutable_buffer &buffer,
                     udp::endpoint &remote_endpoint,
                     std::size_t &bytes_received)
{
    boost::system::error_code ec;
    udp::endpoint receive_endpoint;
    bytes_received = socket.async_receive_from(buffer, receive_endpoint,
                                               yield[ec]);
    if(remote_endpoint == any_endpoint){
        remote_endpoint = receive_endpoint;
    } else if (remote_endpoint != receive_endpoint){
        //discard this packet
        std::cerr << "Received message from unknown endpoint" << std::endl;
        return Status::networkFailure;
    }
    if(ec){
        return Status::networkFailure;
    }
    return Status::success;
}

std::variant<Status, packet::data_packet>
    receiveData(boost::asio::yield_context &yield,
                boost::asio::io_context &ioc,
                udp::socket &socket,
                const std::size_t& block_size,
                udp::endpoint &remote_endpoint
                )
{
    std::vector<uint8_t> recv_buffer(
            block_size + packet::opcode_len + packet::block_number_len);
    std::size_t bytes_received;
    auto asio_buffer = boost::asio::mutable_buffer(recv_buffer.data(),
                                              recv_buffer.size());
    receivePacket(yield, ioc, socket,
                  asio_buffer,
                  remote_endpoint,
                  bytes_received);
    recv_buffer.resize(bytes_received);
    packet::data_packet data(recv_buffer);
    return data;
}

Status sendAck(boost::asio::yield_context &yield,
                       udp::socket &socket,
                       const udp::endpoint &remote_endpoint,
                       const uint16_t &blockNumber){
    packet::ack_packet packet = {blockNumber};
    auto buffer = packet.buffer();
    boost::system::error_code ec;
    std::size_t txSize = socket.async_send_to(
            boost::asio::mutable_buffer(buffer.data(), buffer.size()),
            remote_endpoint,
            yield[ec]);
    if(ec || txSize != buffer.size()){
        std::cerr << ec << std::endl;
        return Status::networkFailure;
    }
    return Status::success;
}

Status downloadFile(boost::asio::yield_context &yield,
                    boost::asio::io_context &ioc,
                    const std::string &filename,
                    udp::endpoint &remote_endpoint,
                    std::ostream &out){
    udp::socket socket(ioc);
    socket.open(remote_endpoint.protocol());
    Status st = Status::unknownError;
    st = sendReadRequest(yield, socket, remote_endpoint, filename);
    if(st != Status::success){
        return st;
    }
    std::vector<char> receiveVector(BLOCK_SIZE);
    uint16_t block_number = 1;
    udp::endpoint remote_tid;
    while(1) {
        auto ret = receiveData(yield, ioc, socket, BLOCK_SIZE, remote_tid);
        if(std::get_if<Status>(&ret)){
            std::cerr << "Failed to receive Response" << std::endl;
            return Status::networkFailure;
        }
        packet::data_packet* data_resp = std::get_if<packet::data_packet>(&ret);
        if(data_resp->block_number != block_number){
            std::cerr << "Received invalid block number "
                      << "expected :" << block_number
                      << "Received :" << data_resp->block_number << std::endl;
            return Status::networkFailure;
        }
        out.write(reinterpret_cast<const char*>(data_resp->data.data()),
                  data_resp->data.size());
        st = sendAck(yield, socket, remote_tid, block_number);
        if(st != Status::success){
            return st;
        }
        if(data_resp->data.size() != BLOCK_SIZE){
            break;
        } else {
            ++block_number;
        }
    }
    return Status::success;
}
} //namespace tftp

using boost::asio::ip::udp;
int main(){
    std::cout << "Hello World!" << std::endl;
    boost::asio::io_context ioc;
    udp::resolver resolver(ioc);
    udp::endpoint remote_endpoint = *resolver.resolve(udp::v4(), "localhost", "12345").begin();
    std::string filename("testfile", std::ios::out | std::ios::binary);
    std::ofstream file_handle(filename, std::ios::out | std::ios::binary);
    boost::asio::spawn([&](boost::asio::yield_context yield){
            tftp::downloadFile(yield, ioc, "tftp.py", remote_endpoint,
                               file_handle);
    });
    ioc.run();
    std::cout << "Bye World!" << std::endl;
    return 0;
}
