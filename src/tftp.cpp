#include <iostream>
#include <vector>
#include <string>
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>

#include "packet.hpp"

namespace tftp {

enum class Status: int {
    success = 0,
    unknownError,
    networkFailure,
};

constexpr size_t BLOCK_SIZE = 512;

template<typename T>
void arrDelete(T* data){
    delete[] data;
}

using boost::asio::ip::udp;

Status sendReadRequest(boost::asio::yield_context &yield,
                       udp::socket &socket,
                       const udp::endpoint &remoteEndpoint,
                       const std::string &filename){
    packet::rrq_packet packet = {filename};
    auto buffer = packet.buffer();
    boost::system::error_code ec;
    std::cout << remoteEndpoint << std::endl;
    std::size_t txSize = socket.async_send_to(
            boost::asio::mutable_buffer(buffer.data(), buffer.size()),
            remoteEndpoint,
            yield[ec]);
    if(ec || txSize != buffer.size()){
        std::cerr << ec << std::endl;
        return Status::networkFailure;
    }
    return Status::success;
}

Status receiveData(boost::asio::yield_context &yield,
                   boost::asio::io_context &ioc,
                   udp::socket &socket,
                   const uint16_t blockNumber,
                   boost::asio::mutable_buffer &buffer,
                   std::size_t &bytesReceived
                   )
{
    boost::system::error_code ec;
    std::size_t recvSize  = socket.async_receive(buffer, yield[ec]);
    if(ec){
        return Status::networkFailure;
    }
    bytesReceived = recvSize;
    return Status::success;
}

Status sendAck(boost::asio::yield_context &yield,
                       udp::socket &socket,
                       const udp::endpoint &remoteEndpoint,
                       const uint16_t &blockNumber){
    packet::ack_packet packet = {blockNumber};
    auto buffer = packet.buffer();
    boost::system::error_code ec;
    std::size_t txSize = socket.async_send_to(
            boost::asio::mutable_buffer(buffer.data(), buffer.size()),
            remoteEndpoint,
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
                    udp::endpoint &remoteEndpoint,
                    std::ostream &out){
    udp::socket socket(ioc);
    socket.open(remoteEndpoint.protocol());
    Status st = Status::unknownError;
    st = sendReadRequest(yield, socket, remoteEndpoint, filename);
    if(st != Status::success){
        return st;
    }
    std::vector<char> receiveVector(BLOCK_SIZE);
    std::size_t bytesReceived;
    uint16_t blockNumber = 0;
    do {
        auto asioBuffer = boost::asio::mutable_buffer(receiveVector.data(), BLOCK_SIZE);
        st = receiveData(yield,
                         ioc,
                         socket,
                         blockNumber,
                         asioBuffer,
                         bytesReceived);
        if(st != Status::success){
            return st;
        }
        out.write(receiveVector.data(),bytesReceived);
        st = sendAck(yield,
                     socket,
                     remoteEndpoint,
                     blockNumber);
        if(st != Status::success){
            return st;
        }
    } while(bytesReceived != BLOCK_SIZE);
    return Status::success;
}
} //namespace tftp

using boost::asio::ip::udp;
int main(){
    std::cout << "Hello World!" << std::endl;
    boost::asio::io_context ioc;
    udp::resolver resolver(ioc);
    udp::endpoint remoteEndpoint = *resolver.resolve(udp::v4(), "localhost", "12345").begin();
    boost::asio::spawn([&](boost::asio::yield_context yield){
            tftp::downloadFile(yield, ioc, "testfile", remoteEndpoint, std::cout);
    });
    ioc.run();
    std::cout << "Bye World!" << std::endl;
    return 0;
}
