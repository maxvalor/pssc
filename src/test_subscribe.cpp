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
	node.SetTopicCallback([](std::string topic, std::uint8_t* data, size_t size)
	{
		LOG(INFO) << "topic:" << topic;
		int i;
		memcpy(&i, data, sizeof(int));
		LOG(INFO) << "data:" << i;
		LOG(INFO) << "total size:" << size;
	});
	node.Initialize(20001);
	auto success = node.Subscribe("test_topic");
	LOG(INFO) << "subscribe rlt value:" << success;
	LOG(INFO) << "subscribe: " << (success ? "true" : "false");

	getchar();

	return 0;
}
