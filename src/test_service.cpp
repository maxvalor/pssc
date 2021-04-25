//============================================================================
// Name        : light.cpp
// Author      :
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include "pssc/protocol/Node.h"
#include <stdio.h>

int main(int argc, char*argv[]) {
	pssc::Node node;
//	node.SetServiceCallback([&](std::string srv, std::uint64_t mid, std::uint64_t client_id, std::uint8_t* data, size_t size)
//	{
//		LOG(INFO) << "srv:" << srv;
//		node.SendCallResult(client_id, mid, true, nullptr, 0);
//	});
	node.Initialize(std::string(argv[1]), 20001);
	LOG(INFO) << "advertise service: " << (client.AdvertiseService("a") ? "true" : "false");

	getchar();

	return 0;
}
