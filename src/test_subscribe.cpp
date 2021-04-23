//============================================================================
// Name        : light.cpp
// Author      :
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include "light/pssc/PsscClient.h"
#include <stdio.h>

int main(int argc, char*argv[]) {
	pssc::PsscClient client;
	client.SetTopicCallback([](std::string topic, std::uint8_t* data, size_t size)
	{
		LOG(INFO) << "topic:" << topic;
		int i;
		memcpy(&i, data, size);
		LOG(INFO) << "data:" << i;
	});
	client.Initialize(std::string(argv[1]), 20001);
	auto success = client.Subscribe("test_topic");
	LOG(INFO) << "subscribe rlt value:" << success;
	LOG(INFO) << "subscribe: " << (success ? "true" : "false");

	getchar();

	return 0;
}
