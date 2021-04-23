/*
 * node.h
 *
 *  Created on: Mar 9, 2021
 *      Author: ubuntu
 */

#ifndef LIGHT_PSSC_NODE_H_
#define LIGHT_PSSC_NODE_H_

#include <string>
#include <functional>
#include <glog/logging.h>
#include "light/tcp/tcp_client.h"
#include "pssc/instruction.h"
#include <thread>
#include <unordered_map>
#include <list>

namespace light
{

class Node
{
public:
	void Init(std::string name)
	{
		this->name = name;
		client = std::make_shared<light::TCPClient>(
			20001,
			[this](std::shared_ptr<light::TCPConnection> conn)
			{
				DLOG(INFO) << "connected.";
				this->conn = conn;

				std::thread([this]()
				{
					std::string nodeName = this->name;

					std::string name = nodeName;
					auto req = light::TCPMessage::Generate(name.size() + 1);
					req->header.id = 0;
					req->header.ins = Ins::REGISTER;
					req->header.type = light::TCPMessage::REQUEST;
					req->header.bodyLength = name.size() + 1;
					memcpy(req->body(), name.c_str(), name.size());
					memcpy(req->body() + name.size(), name.c_str(), '\0');
					this->conn->SendRequest(req);

					std::shared_ptr<light::TCPMessage> resp;
					this->conn->WaitForResponse(resp);
					auto OK = (bool)resp->body()[0];
					if (!OK)
					{
						LOG(FATAL) << "Failed to register node";
					}

					this->cv.notify_one();
				}).detach();

				DLOG(INFO) << "start connection.";
			},
			[](std::shared_ptr<light::TCPConnection> conn)
			{
				DLOG(INFO) << "disconnected.";
			}
		);

		std::thread([this](){
			DLOG(INFO) << "start service.";
			client->Connect();
		}).detach();

		std::unique_lock<std::mutex> lck(mtx);
		cv.wait(lck);
	}

	void Spin()
	{
		while (1)
		{
			std::shared_ptr<light::TCPMessage> req;
			conn->WaitForRequest(req);
			if (req->header.ins == Ins::PUBLISH)
			{
				auto offset = req->body();

				// get topic name
				size_t topicLen;
				memcpy(&topicLen, offset, sizeof(size_t));
				char ctopic[topicLen + 1];
				memcpy(ctopic, offset + sizeof(size_t), topicLen);
				ctopic[topicLen] = '\0';
				std::string topic(ctopic);

				offset = offset + sizeof(size_t) + topicLen;

				// get data name
				size_t dataLen;
				memcpy(&dataLen, offset, sizeof(size_t));
				std::uint8_t cdata[dataLen + 1];
				memcpy(cdata, offset + sizeof(size_t), dataLen);

				auto resp = light::TCPMessage::Generate(1);
				resp->header.id = req->header.id;
				resp->header.ins = Ins::PUBLISH;
				resp->header.type = light::TCPMessage::RESPONSE;
				resp->header.bodyLength = 1;

				resp->body()[0] = 1;

				conn->SendResponse(resp);

				{
					std::lock_guard<std::mutex> lck(mtxSub);
					auto r = subscribers.find(topic);
					if (r != subscribers.end())
					{
						for (auto& f : r->second)
						{
							f(cdata, dataLen);
						}
					}
				}

			}
		}
	}

	void Subscribe(std::string topic, std::function<void(std::uint8_t*, size_t size)> f)
	{
		std::string name = topic;
		std::string nodeName = this->name;

		auto req = light::TCPMessage::Generate(
				sizeof(size_t) * 2 + nodeName.size() + name.size()
		);
		req->header.id = 0;
		req->header.ins = Ins::SUBSCRIBE;
		req->header.type = light::TCPMessage::REQUEST;
		req->header.bodyLength = sizeof(size_t) * 2 + nodeName.size() + name.size();

		auto offset = req->body();
		// set node name
		auto size = nodeName.size();
		memcpy(offset, &size, sizeof(size_t));
		memcpy(offset + sizeof(size_t), nodeName.c_str(), size);
		offset = offset + sizeof(size_t) + size;

		// set topic
		size = name.size();
		memcpy(offset, &size, sizeof(size_t));
		memcpy(offset + sizeof(size_t), name.c_str(), size);

		for (int i = 0; i < req->header.bodyLength; ++i)
		{
			DLOG(INFO) << std::to_string(*(req->body() + i));
		}

		conn->SendRequest(req);

		std::shared_ptr<light::TCPMessage> resp;
		conn->WaitForResponse(resp);
		DLOG(INFO) << "SUBSCRIBE response: " << (bool)resp->body()[0];

		if ((bool)resp->body()[0])
		{
			std::lock_guard<std::mutex> lck(mtxSub);
			auto r = subscribers.find(topic);
			if (r == subscribers.end())
			{
				std::list<std::function<void(std::uint8_t*, size_t)>> funcs;
				funcs.push_back(f);
				subscribers.insert(std::pair<std::string, std::list<std::function<void(std::uint8_t*, size_t)>>>(topic, funcs));
			}
			else
			{
				r->second.push_back(f);
			}
		}


	}

	void Publish(std::string topic, std::uint8_t* data, size_t size)
	{
		std::string& topicName = topic;
		auto req = light::TCPMessage::Generate(topicName.size() + size + 2 * sizeof(size_t));
		req->header.id = 0;
		req->header.ins = Ins::PUBLISH;
		req->header.type = light::TCPMessage::REQUEST;
		req->header.bodyLength = topicName.size() + size + 2 * sizeof(size_t);

		auto offset = req->body();
		// set topic name
		auto sizeTopic = topicName.size();
		memcpy(offset, &sizeTopic, sizeof(size_t));
		memcpy(offset + sizeof(size_t), topicName.c_str(), sizeTopic);
		offset = offset + sizeof(size_t) + sizeTopic;

		// set data
		memcpy(offset, &size, sizeof(size_t));
		memcpy(offset + sizeof(size_t), data, size);

		conn->SendRequest(req);
		std::shared_ptr<light::TCPMessage> resp;
		conn->WaitForResponse(resp);
		DLOG(INFO) << "PUBLISH response: " << (bool)resp->body()[0];
	}

	void DeInit() {}

private:
	std::string name;
	std::shared_ptr<light::TCPConnection> conn;
	std::shared_ptr<light::TCPClient> client;
	std::mutex mtx;
	std::condition_variable cv;

	std::mutex mtxSub;
	std::unordered_map<std::string, std::list<std::function<void(std::uint8_t*, size_t)>>> subscribers;
};

}

#endif /* LIGHT_PSSC_NODE_H_ */
