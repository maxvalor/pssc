//============================================================================
// Name        : light.cpp
// Author      : 
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <glog/logging.h>
#include <thread>
#include "../include/light/tcp/TCPService.h"

int main() {
	light::TCPService service(
		20001,
		[](std::shared_ptr<light::TCPConnection> conn)
		{
			DLOG(INFO) << "connected.";

			std::thread([conn]()
			{
				std::shared_ptr<light::TCPMessage> req;
				conn->WaitForRequest(req);
				DLOG(INFO) << "request.";
				req->header.type = light::TCPMessage::RESPONSE;
				conn->SendResponse(req);

			}).detach();

			DLOG(INFO) << "start connection.";
		},
		[](std::shared_ptr<light::TCPConnection> conn)
		{
			DLOG(INFO) << "disconnected.";
		}
	);
	DLOG(INFO) << "start service.";
	service.Start();
	return 0;
}
