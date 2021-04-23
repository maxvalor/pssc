/*
 * tcp_client.cpp
 *
 *  Created on: Mar 9, 2021
 *      Author: ubuntu
 */

#include "tcp/tcp_client.h"
#include <glog/logging.h>

namespace light {

TCPClient::TCPClient(
	  int port,
	  std::function<void(std::shared_ptr<TCPConnection>)> on_connected,
	  std::function<void(std::shared_ptr<light::TCPConnection>)> on_disconnected
	  )
  : _ep(tcp::v4(), port), _socket(_ioContext)
{
	OnConnected = on_connected;
	OnDisconnected = on_disconnected;
}

void TCPClient::Run()  {
	boost::system::error_code ec;
	boost::asio::io_service::work work(_ioContext);
	_ioContext.run(ec);
	DLOG(ERROR) << "Run Finish: " << boost::diagnostic_information(ec);
}

bool TCPClient::Connect()
{
	_socket.connect(_ep);
    tcp::no_delay option(true);
    _socket.set_option(option);
	auto conn = std::make_shared<TCPConnection>(std::move(_socket), OnDisconnected);
	conn->Start();
	OnConnected(conn);
	Run();
	return true;
}

}
