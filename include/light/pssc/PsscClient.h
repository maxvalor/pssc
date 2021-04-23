/*
 * PsscClient.h
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
#include "light/tcp/tcp_client.h"
#include "id_generator.h"
#include "instruction.h"
#include "Notifier.h"

namespace pssc {

class PsscClient
{
	std::shared_ptr<light::TCPClient> client;
	std::shared_ptr<light::TCPConnection> conn;
	std::uint64_t clientId;
	IDGenerator<std::uint64_t> idGen;
	bool running;
	std::unordered_map<std::uint64_t, std::function<void()>> callResps;

	Notifier startNoti;

	std::mutex mtxMsg;
	std::condition_variable cvMsg;
	std::mutex mtxCall;
	std::condition_variable cvCall;
	std::list<std::shared_ptr<light::TCPMessage>> msgList;
	std::list<std::shared_ptr<light::TCPMessage>> callList;

	std::mutex mtxResp;
	std::unordered_map<std::uint64_t, std::shared_ptr<light::TCPMessage>> respList;


	std::function<void(std::string, std::uint8_t*, size_t)> topicCallback;
	// srv_name, message_id, caller_client_id, data, size_of_data
	using CallFunc = std::function<void(std::string, std::uint64_t, std::uint64_t, std::uint8_t*, size_t)>;
	CallFunc srvCallback;

private:

	bool SendRequestAndWaitForResponse(std::uint64_t id, std::shared_ptr<light::TCPMessage> req, std::shared_ptr<light::TCPMessage>& resp)
	{
		auto noti = std::make_shared<Notifier>();
		auto f = [noti]()
		{
			noti->notify_one();
		};

		mtxResp.lock();
		callResps.insert(std::pair<std::uint64_t, std::function<void()>>(id, f));
		mtxResp.unlock();
		conn->SendRequest(req);

		auto rlt = noti->wait_for(std::chrono::milliseconds(3000));
		if (rlt == std::cv_status::timeout)
		{
			return false;
		}
		std::lock_guard<std::mutex> lck(mtxResp);
		resp = respList.at(id);
		return true;
	}

public:
	PsscClient()
	{
		topicCallback = [](std::string, std::uint8_t*, size_t){};
		srvCallback = [](std::string, std::uint64_t, std::uint64_t, std::uint8_t*, size_t){};
	}

	bool Initialize(std::string name, int port)
	{
		client = std::make_shared<light::TCPClient>(
			port,
			[this, name](std::shared_ptr<light::TCPConnection> conn)
			{
				this->conn = conn;
				DLOG(INFO) << "connected.";


				std::thread([this, name](){
					// | INS | ID | SIZE_OF_TOPIC | TOPIC |
					auto req = light::TCPMessage::Generate(
							1
							+ ID_SIZE
							+ SIZE_SIZE
							+ name.size()
							);
					req->AppendData((std::uint8_t)Ins::REGISTER);
					req->AppendData(idGen.next());
					req->AppendData(name);

					this->conn->SendRequest(req);

					std::shared_ptr<light::TCPMessage> resp;
					this->conn->WaitForResponse(resp);

					// | INS | ID | SUCCESS | CLIENT_ID |
					resp->IgnoreData(INS_SIZE + ID_SIZE);
					bool success;
					resp->NextData(success);
					if (success)
					{
						resp->NextData(this->clientId);
						LOG(INFO) << "Success to register node: " << this->clientId;
						startNoti.notify_one();
					}
					else
					{
						LOG(WARNING) << "failed.";
					}
				}).detach();
			},
			[this](std::shared_ptr<light::TCPConnection> conn)
			{
				running = false;
				DLOG(INFO) << "disconnected.";
			}
		);
		std::thread([this]()
		{
			client->Connect();
		}).detach();

		auto&& rlt = startNoti.wait_for(std::chrono::milliseconds(3000));
		if (rlt == std::cv_status::timeout)
		{
			LOG(ERROR) << "Failed to register node";
			return false;
		}

		running = true;

		// response thread
		std::thread([this]()
		{
			while (running)
			{
				std::shared_ptr<light::TCPMessage> resp;
				conn->WaitForResponse(resp);
				std::lock_guard<std::mutex> lck(mtxResp);
				// | INS | ID | ... |
				resp->IgnoreData(INS_SIZE);
				std::uint64_t id;
				resp->NextData(id);
				LOG(INFO) << "response id:" << id;
				resp->Reset();
				respList.insert(std::pair<std::uint64_t, std::shared_ptr<light::TCPMessage>>(id, resp));
				try {
					auto noti_f = callResps.at(id);
					noti_f();
				} catch (...) {
				}
			}
		}).detach();


		// publish and service call thread
		std::thread([this]()
		{
			while (running)
			{
				std::shared_ptr<light::TCPMessage> req;
				conn->WaitForRequest(req);
				// | INS | ID | SIZE_OF_TOPIC | TOPIC | SIZE_OF_DATA | DATA |
				std::uint8_t ins;
				req->NextData(ins);
				switch (ins)
				{
					case Ins::PUBLISH:
					{
						mtxMsg.lock();
						req->Reset();
						msgList.push_back(req);
						mtxMsg.unlock();
						cvMsg.notify_one();
						break;
					}
					case Ins::SERVICE_CALL:
					{
						mtxCall.lock();
						req->Reset();
						callList.push_back(req);
						mtxCall.unlock();
						cvCall.notify_one();
						break;
					}
				}
			}
		}).detach();

		// message dispatch thread
		std::thread([this]()
		{
			while (running)
			{
				std::unique_lock<std::mutex> lck(mtxMsg);
				if (msgList.empty())
				{
					cvMsg.wait(lck);
				}
				// | INS | ID | PUBLISHER_ID |SIZE_OF_TOPIC | TOPIC | SIZE_OF_DATA | DATA | FEEDBACK |
				auto& req = *msgList.begin();

				req->IgnoreData(INS_SIZE + ID_SIZE + ID_SIZE);


				std::string srv;
				size_t sizeOfData;


				req->NextData(srv);
				req->NextData(sizeOfData);
				topicCallback(srv, req->cur(), sizeOfData);
				msgList.pop_front();
			}
		}).detach();

		// call dispatch thread
		std::thread([this]()
		{
			while (running)
			{

				std::unique_lock<std::mutex> lck(mtxCall);
				if (callList.empty())
				{
					cvCall.wait(lck);
				}

				// | INS | ID | CLIENT_ID | SIZE_OF_SRV_NAME | SRV_NAME | SIZE_OF_DATA | DATA |
				auto& req = *callList.begin();

				std::uint64_t mid, client_id;
				std::string srv;
				size_t sizeOfData;

				req->IgnoreData(INS_SIZE);
				req->NextData(mid);
				req->NextData(client_id);
				req->NextData(srv);
				req->NextData(sizeOfData);
				if (sizeOfData > 0)
				{
					srvCallback(srv, mid, client_id, req->cur(), sizeOfData);
				}
				else
				{
					srvCallback(srv, mid, client_id, nullptr, 0);
				}

				callList.pop_front();
			}
		}).detach();
		return true;
	}
	// topic
	void Publish(std::string topic, std::uint8_t*data, size_t size, bool feedback = false)
	{
		// | INS | ID | PUBLISHER_ID | SIZE_OF_TOPIC | TOPIC | SIZE_OF_DATA | DATA | FEEDBACK |
		auto req = light::TCPMessage::Generate(
				INS_SIZE
				+ ID_SIZE
				+ ID_SIZE
				+ SIZE_SIZE
				+ topic.size()
				+ SIZE_SIZE
				+ size
				+ sizeof(bool)
		);

		req->AppendData((std::uint8_t)Ins::PUBLISH);
		req->AppendData(idGen.next());
		req->AppendData(clientId);
		req->AppendData(topic);
		req->AppendData(size);
		req->AppendData(data, size);
		req->AppendData(feedback);

		conn->SendRequest(req);
	}

	bool Subscribe(std::string topic)
	{
		// | INS | ID | SUBSCRIBER_ID | SIZE_OF_TOPIC | TOPIC |
		auto req = light::TCPMessage::Generate(
				INS_SIZE
				+ ID_SIZE
				+ ID_SIZE
				+ SIZE_SIZE
				+ topic.size()
		);

		std::uint64_t id = idGen.next();

		req->AppendData((std::uint8_t)Ins::SUBSCRIBE);
		req->AppendData(id);
		req->AppendData(clientId);
		req->AppendData(topic);

		std::shared_ptr<light::TCPMessage> resp;
		if(!SendRequestAndWaitForResponse(id, req, resp))
		{
			return false;
		}
		conn->SendRequest(req);

//		conn->WaitForResponse(resp);

//		std::uint8_t ins;
//		std::uint64_t rid;
//		resp->NextData(ins);
//		resp->NextData(rid);
//		LOG(INFO) << "ins:" << (std::uint32_t)ins;
//		LOG(INFO) << "message id:" << id;
//		LOG(INFO) << "message rid:" << rid;
		// | INS | ID | SUCCESS |
		resp->IgnoreData(INS_SIZE + ID_SIZE);

		bool success;
		resp->NextData(success);

		return success;
	}

	bool AdvertiseService(std::string srv)
	{
		// | INS | ID | CLIENT_ID | SIZE_OF_SRV_NAME | SRV_NAME |
		auto req = light::TCPMessage::Generate(
				INS_SIZE
				+ ID_SIZE
				+ ID_SIZE
				+ SIZE_SIZE
				+ srv.size()
		);

		auto id = idGen.next();
		req->AppendData((std::uint8_t)Ins::ADDVERTISE_SERVICE);
		req->AppendData(id);
		req->AppendData(clientId);
		req->AppendData(srv);

		std::shared_ptr<light::TCPMessage> resp;
		if(!SendRequestAndWaitForResponse(id, req, resp))
		{
			LOG(WARNING) << "Timeout in AdvertiseService.";
			return false;
		}

		// | INS | ID | SUCCESS |
		bool success;
		resp->IgnoreData(INS_SIZE + ID_SIZE);
		resp->NextData(success);
		return success;
	}

	// call service
	bool RemoteCall(std::string srv, std::uint8_t*data, size_t size,
			std::uint8_t* &resp_data, size_t& resp_size)
	{
		// | INS | ID | CLIENT_ID | SIZE_OF_SRV_NAME | SRV_NAME | SIZE_OF_DATA | DATA |
		auto req = light::TCPMessage::Generate(
				INS_SIZE
				+ ID_SIZE
				+ ID_SIZE
				+ SIZE_SIZE
				+ srv.size()
				+ SIZE_SIZE
				+ size
		);

		auto id = idGen.next();
		req->AppendData((std::uint8_t)Ins::SERVICE_CALL);
		req->AppendData(id);
		req->AppendData(clientId);
		req->AppendData(srv);
		req->AppendData(size);
		req->AppendData(data, size);

		std::shared_ptr<light::TCPMessage> resp;
		if(!SendRequestAndWaitForResponse(id, req, resp))
		{
			return false;
		}

		// | INS | ID | SUCCESS |
		bool success;
		resp->IgnoreData(INS_SIZE + ID_SIZE);
		resp->NextData(success);
		return success;

//		// | INS | ID | SUCCESS | CLIENT_ID | SIZE_OF_SRV_NAME | SRV_NAME | SIZE_OF_DATA | DATA |
//		resp->IgnoreData(INS_SIZE + sizeof(id) + sizeof(clientId) + SIZE_SIZE + srv.size());
//		resp->NextData(resp_size);
//		resp->NextData(resp_data, resp_size);
//		return true;
	}

	void SendCallResult(std::uint64_t client_id, std::uint64_t mid, bool success, std::uint8_t* data, size_t size)
	{
		// | INS | ID | CLIENT_ID | SUCCESS | SIZE_OF_DATA | DATA |
		auto req = light::TCPMessage::Generate(
				INS_SIZE
				+ ID_SIZE
				+ ID_SIZE
				+ sizeof(bool)
				+ SIZE_SIZE
				+ size
		);

		req->AppendData((std::uint8_t)Ins::SERVICE_RESPONSE);
		req->AppendData(mid);
		req->AppendData(client_id);
		req->AppendData(success);
		req->AppendData(size);
		if (size > 0)
		{
			req->AppendData(data, size);
		}

		conn->SendRequest(req);
	}

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
