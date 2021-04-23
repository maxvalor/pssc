/*
 * tcp_client.h
 *
 *  Created on: Mar 9, 2021
 *      Author: ubuntu
 */

#ifndef LIGHT_TCP_TCP_CLIENT_H_
#define LIGHT_TCP_TCP_CLIENT_H_

#include "tcp_connection.h"
#include <boost/asio.hpp>
#include <boost/exception/diagnostic_information.hpp>

namespace light {

class TCPClient
{
public:
	TCPClient(
	  int port,
	  std::function<void(std::shared_ptr<TCPConnection>)> on_connected,
	  std::function<void(std::shared_ptr<light::TCPConnection>)> on_disconnected
	);

	bool Connect();
	inline void Disconnect() { _ioContext.stop(); }

private:
	boost::asio::io_service _ioContext;
	tcp::socket _socket;
	tcp::endpoint _ep;

	std::function<void(std::shared_ptr<TCPConnection>)> OnConnected;
	std::function<void(std::shared_ptr<light::TCPConnection>)> OnDisconnected;

	void Run();
};

}


#endif /* LIGHT_TCP_TCP_CLIENT_H_ */
