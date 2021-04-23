//============================================================================
// Name        : light.cpp
// Author      : 
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include "light/pssc/PsscClient.h"

int main(int argc, char*argv[]) {
	pssc::PsscClient client;
	client.Initialize(std::string(argv[1]), 20001);
	client.Subscribe("test_topic");

	return 0;
}
