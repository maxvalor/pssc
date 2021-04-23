/*
 * service.h
 *
 *  Created on: Mar 8, 2021
 *      Author: ubuntu
 */

#ifndef LIGHT_TCP_TCP_SERVICE_H_
#define LIGHT_TCP_TCP_SERVICE_H_

#include <functional>
#include <memory>
#include <boost/asio.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <light/tcp/tcp_connection.h>
#include <iostream>
#include <atomic>

using boost::asio::ip::tcp;

namespace light {
class TCPService
{
private:
	static const std::uint64_t MAX_CONNECTIONS = 100;
public:
	TCPService(
		  int port,
		  std::function<void(std::shared_ptr<TCPConnection>)> on_connected,
		  std::function<void(std::shared_ptr<light::TCPConnection>)> on_disconnected
		  );

	inline void Start() { Run(); }
	inline void Stop() { _ioContext.stop(); }

private:
	void Accept();

private:
	std::atomic<int> _currentConnections;
	boost::asio::io_service _ioContext;
	tcp::acceptor _acceptor;
	tcp::socket _socket;

	std::function<void(std::shared_ptr<TCPConnection>)> OnConnected;
	std::function<void(std::shared_ptr<light::TCPConnection>)> OnDisconnected;

	void Run();
};
}


#endif /* LIGHT_TCP_TCP_SERVICE_H_ */
