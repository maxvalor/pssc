//============================================================================
// Name        : light.cpp
// Author      :
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include "light/pssc/PsscClient.h"
#include <stdio.h>

#define SIZE_A 10

int main(int argc, char*argv[]) {
	pssc::PsscClient client;
	client.Initialize(std::string(argv[1]), 20001);
	std::uint8_t* data, *resp_data;
	data = new std::uint8_t[SIZE_A];
	size_t resp_size;
	LOG(INFO) << "call service start.";
	auto success = client.RemoteCall("a", data, SIZE_A, resp_data, resp_size);
	LOG(INFO) << "call service: " << (success ? "true" : "false");
//
//	for (int i = 0; i < 10; ++i)
//	{
//		LOG(INFO) << "call service start:" + i;
//		auto success = client.RemoteCall("test_srv", data, 10u, resp_data, resp_size);
//		LOG(INFO) << "call service: " << (success ? "true" : "false");
//	}

	getchar();

	return 0;
}
