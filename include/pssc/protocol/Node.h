/*
 * Node.h
 *
 *  Created on: Apr 21, 2021
 *      Author: ubuntu
 */

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <thread>
#include <functional>
#include <glog/logging.h>
#include "pssc/transport/tcp/TCPClient.h"
#include "pssc/util/IDGenerator.h"
#include "Instruction.h"
#include "types.h"
#include "pssc/util/Notifier.h"

namespace pssc {

using trs::TCPMessage;
using trs::TCPClient;
using trs::TCPConnection;

class Node
{
	std::shared_ptr<TCPClient> client;
	std::shared_ptr<TCPConnection> conn;
	std::uint64_t nodeId;
	IDGenerator<std::uint64_t> messageIdGen;
	bool running;
	std::unordered_map<pssc_id, std::function<void()>> mapAckNoti;

	util::Notifier startNoti;

	std::mutex mtxMsg;
	std::condition_variable cvMsg;
	std::mutex mtxCall;
	std::condition_variable cvCall;
	std::list<std::shared_ptr<TCPMessage>> msgList;
	std::list<std::shared_ptr<TCPMessage>> callList;

	std::mutex mtxAcks;
	std::unordered_map<std::uint64_t, std::shared_ptr<TCPMessage>> acks;


	std::function<void(std::string, std::uint8_t*, size_t)> topicCallback;
	// srv_name, message_id, caller_client_id, data, size_of_data
	using CallFunc = std::function<void(std::string, std::uint64_t, std::uint64_t, std::uint8_t*, size_t)>;
	CallFunc srvCallback;

private:

	bool SendRequestAndWaitForResponse(pssc_id messageId, std::shared_ptr<TCPMessage> req, std::shared_ptr<TCPMessage>& resp);

	void OnConntected(std::shared_ptr<TCPConnection> conn);
	void OnDisconntected(std::shared_ptr<TCPConnection> conn);
	void DispatchMessage(std::shared_ptr<TCPMessage> msg);

	void OnPublish(std::shared_ptr<TCPMessage> msg);

public:
	Node()
	{
		topicCallback = [](std::string, std::uint8_t*, size_t){};
		srvCallback = [](std::string, std::uint64_t, std::uint64_t, std::uint8_t*, size_t){};
	}

	bool Initialize(int port);
	void Publish(std::string topic, std::uint8_t* data, size_t size, bool feedback = false);
	bool Subscribe(std::string topic);
	bool AdvertiseService(std::string srv_name);

	// on message received
	void SetTopicCallback(std::function<void(std::string, std::uint8_t*, size_t)> topicCallback)
	{
		this->topicCallback = topicCallback;
	}

	// on service call received
	void SetServiceCallback(CallFunc srvCallback)
	{
		this->srvCallback = srvCallback;
	}
};

};
