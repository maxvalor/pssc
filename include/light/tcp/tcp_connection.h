/*
 * tcp_connection.h
 *
 *    Created on: Mar 8, 2021
 *            Author: ubuntu
 */

#ifndef LIGHT_TCP_TCPCONNECTION_H_
#define LIGHT_TCP_TCPCONNECTION_H_

#include <boost/asio.hpp>
#include <condition_variable>
#include <functional>
#include <list>
#include <atomic>

#include "tcp_message.h"
#include "Locker.h"

using boost::asio::ip::tcp;

namespace light {

class TCPConnection : public std::enable_shared_from_this<TCPConnection>
{
	TCPConnection() = default;
public:
	TCPConnection(const TCPConnection&) = default;
	TCPConnection(tcp::socket socket, std::function<void(std::shared_ptr<light::TCPConnection>)> on_disconnected);
	virtual ~TCPConnection();

	bool SendRequest(std::shared_ptr<TCPMessage>);
	bool SendResponse(std::shared_ptr<TCPMessage>);

	bool WaitForRequest(std::shared_ptr<TCPMessage>&);
	bool WaitForResponse(std::shared_ptr<TCPMessage>&);

	void Start();

	inline bool IsRunning() { return running; }

private:
    std::function<void(std::shared_ptr<light::TCPConnection>)> OnDisconnected;

    tcp::socket sock;
    std::shared_ptr<TCPMessage> recvBuffer;
    std::atomic_bool running;
    std::mutex mtxSend, mtxRecvReq,mtxRecvResp;
    std::condition_variable cvRecvReq, cvRecvResp;

//    read_write_lock rwlckReq, rwlckResp;

    std::list<std::shared_ptr<TCPMessage>> requests;
    std::list<std::shared_ptr<TCPMessage>> responses;

    void ReadHeader();
    void ReadBody();
    bool Write(std::shared_ptr<TCPMessage>);
    bool Write(std::uint8_t* data, size_t size);
};
}



#endif /* LIGHT_TCP_TCPCONNECTION_H_ */
