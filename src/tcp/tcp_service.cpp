/*
 * tcp_service.cpp
 *
 *  Created on: Mar 9, 2021
 *      Author: ubuntu
 */

#include "tcp/tcp_service.h"
#include <glog/logging.h>

namespace light {

TCPService::TCPService(
	  int port,
	  std::function<void(std::shared_ptr<TCPConnection>)> on_connected,
	  std::function<void(std::shared_ptr<light::TCPConnection>)> on_disconnected
	  )
  : _acceptor(_ioContext, tcp::endpoint(tcp::v4(), port)), _socket(_ioContext)
{
	_currentConnections = 0;
	OnConnected = on_connected;
	OnDisconnected = on_disconnected;

	Accept();
}

void TCPService::Run()  {
	boost::system::error_code ec;
	boost::asio::io_service::work work(_ioContext);
	_ioContext.run(ec);
	DLOG(ERROR) << "Run Finish: " << boost::diagnostic_information(ec);
}

void TCPService::Accept()
{
	_acceptor.async_accept(_socket, [this](boost::system::error_code ec)
	{
	    tcp::no_delay option(true);
	    _socket.set_option(option);

		if (!ec && _currentConnections < MAX_CONNECTIONS)
		{
			++_currentConnections;
			auto connection = std::make_shared<TCPConnection>(
				std::move(_socket),
				[this](std::shared_ptr<light::TCPConnection> conn)
				{
				  --_currentConnections;
				  OnDisconnected(conn);
				}
			);
			connection->Start();

			if (OnConnected != nullptr)
			{
				OnConnected(connection);
			}
		}

		Accept();
	});
}

}




