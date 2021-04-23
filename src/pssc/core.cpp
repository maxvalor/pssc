/*
 * core.cpp
 *
 *  Created on: Mar 9, 2021
 *      Author: ubuntu
 */

#include "pssc/core.h"
#include "pssc/instruction.h"
#include <glog/logging.h>
#include <thread>
#include <string>

namespace light
{

Core::Core(int port)
{
	service = std::make_unique<TCPService>(
			port,
			[this](std::shared_ptr<light::TCPConnection> conn)
			{
				DLOG(INFO) << "client connected.";
				std::thread([this, conn]()
				{
					while (conn->IsRunning())
					{
						std::shared_ptr<TCPMessage> req;

						if (!conn->WaitForRequest(req))
						{
							break;
						}

						std::uint8_t ins;
						req->NextData(ins);

						switch (ins)
						{
							case Ins::REGISTER:
							{
								Register(req, conn);
								break;
							}
							/*case Ins::UNREGISTER:
							{
								std::string name((char*)req->body());
								DLOG(INFO) << "UNREGISTER: " << name;

								auto resp = TCPMessage::Generate(1);
								resp->header.id = req->header.id;
								resp->header.ins = Ins::UNREGISTER;
								resp->header.type = TCPMessage::RESPONSE;
								resp->header.bodyLength = 1;

								if (nodes.find(name) == nodes.end())
								{
									nodes.erase(name);
									DLOG(INFO) << "Unregister node " << name;
									resp->body()[0] = 1;
								}
								else
								{
									DLOG(INFO) << "Not found node " << name;
									resp->body()[0] = 0;
								}

								conn->SendResponse(resp);

								break;
							}
							*/
							case Ins::SUBSCRIBE:
							{
								Subscribe(req, conn);
								break;
							}

							case Ins::PUBLISH:
							{
								Publish(req, conn);
								break;
							}

							case Ins::ADDVERTISE_SERVICE:
							{
								AdvertiseService(req, conn);
								break;
							}

							case Ins::SERVICE_CALL:
							{
								CallService(req, conn);
								break;
							}

							case Ins::SERVICE_RESPONSE:
							{
								ResponseService(req, conn);
								break;
							}
							/*
							default:
							{
								DLOG(WARNING) << "UNKOWN";

								auto resp = TCPMessage::Generate(1);
								resp->header.id = req->header.id;
								resp->header.ins = Ins::UNKOWN;
								resp->header.type = TCPMessage::RESPONSE;
								resp->body()[0] = false;
								conn->SendResponse(resp);
								break;
							}
							*/
						}
					}
				}).detach();
			},
			[this](std::shared_ptr<light::TCPConnection> conn)
			{
				std::string name;

				// remove name->connection
				for (auto & node : nodes)
				{
					if (node.second.get() == conn.get())
					{
						DLOG(INFO) << "node " + node.first + " disconnected.";
						name = node.first;
						nodes.erase(name);
						return;
					}
				}
			}
	);
}

void Core::Publish(std::shared_ptr<TCPMessage> req, std::shared_ptr<light::TCPConnection> conn)
{
	// | INS* | ID | PUBLISHER_ID | SIZE_OF_TOPIC | TOPIC | SIZE_OF_DATA | DATA | FEEDBACK |
	DLOG(WARNING) << "PUBLISH";
	std::uint64_t mid;
	std::string topic;
	std::uint64_t clientId;
	std::string name;
	size_t sizeOfData;
	bool feedback;

	req->NextData(mid);
	req->NextData(clientId);
	req->NextData(topic);
	name = nodeNames.at(clientId);
	req->NextData(sizeOfData);
	req->IgnoreData(sizeOfData);
	req->NextData(feedback);

	auto&& subscribers = topics.find(topic);
	if (subscribers != topics.end())
	{
		for (auto& subscriber : subscribers->second)
		{
			if (name.compare(subscriber) == 0 && !feedback)
			{
				continue;
			}
			DLOG(WARNING) << "publish topic: " + topic + " to node: " + subscriber;
			auto& subConn = nodes.at(subscriber);
			DLOG(WARNING) << "bodyLength: " << req->header.bodyLength;
			subConn->SendRequest(req);
		}
	}
}
void Core::Subscribe(std::shared_ptr<TCPMessage> req, std::shared_ptr<light::TCPConnection> conn)
{
	// | INS* | ID | SUBSCRIBER_ID | SIZE_OF_TOPIC | TOPIC |
	std::uint64_t mid;
	std::string topic;
	std::uint64_t clientId;
	std::string name;

	req->NextData(mid);
	req->NextData(clientId);
	req->NextData(topic);
	name = nodeNames.at(clientId);

	DLOG(INFO) << "SUBSCRIBE: " << name << "," << topic;

	if (topics.find(topic) == topics.end())
	{
		std::list<std::string> subscribers;
		topics.insert(std::pair<std::string, std::list<std::string>>(topic, subscribers));
	}

	try {
		auto& subscribers = topics.at(topic);
		if (std::find(subscribers.begin(), subscribers.end(), name) == subscribers.end())
		{
			subscribers.push_back(name);
			DLOG(INFO) << "SUBSCRIBE: OK.";
		}
	}
	catch (...)
	{
		DLOG(INFO) << "SUBSCRIBE: ???";
	}

	// | INS | ID | SUCCESS |
	auto resp = TCPMessage::Generate(
			INS_SIZE
			+ ID_SIZE
			+ sizeof(bool));

	resp->AppendData(std::uint8_t(Ins::SUBACK));
	resp->AppendData(mid);
	resp->AppendData(true);

	conn->SendResponse(resp);
}

void Core::Register(std::shared_ptr<TCPMessage> req, std::shared_ptr<light::TCPConnection> conn)
{
	// | INS* | ID | SIZE_OF_TOPIC | TOPIC |
	std::string name;
	std::uint64_t id;

	req->NextData(id);
	req->NextData(name);
	DLOG(INFO) << "REGISTER: " << name;

	// | INS | ID | SUCCESS | CLIENT_ID |
	auto resp = TCPMessage::Generate(
			INS_SIZE
			+ ID_SIZE
			+ sizeof(bool)
			+ ID_SIZE);

	resp->AppendData(std::uint8_t(Ins::REGACK));
	resp->AppendData(id);

	if (nodes.find(name) == nodes.end())
	{
		nodes.insert(std::pair<std::string, std::shared_ptr<TCPConnection>>(name, conn));
		auto nodeId = nodeIdGen.next();
		nodeNames.insert(std::pair<std::uint64_t, std::string>(nodeId, name));
		DLOG(INFO) << "Register node " << name;
		resp->AppendData(true);
		resp->AppendData(nodeId);
	}
	else
	{
		DLOG(INFO) << "Already register node " << name;
		resp->AppendData(false);
	}

	conn->SendResponse(resp);
}

void Core::AdvertiseService(std::shared_ptr<TCPMessage> req, std::shared_ptr<light::TCPConnection> conn)
{
	// | INS* | ID | CLIENT_ID | SIZE_OF_SRV_NAME | SRV_NAME |
	std::uint64_t mid;
	std::string srv_name;
	std::uint64_t clientId;

	req->NextData(mid);
	req->NextData(clientId);
	req->NextData(srv_name);

	DLOG(INFO) << "ADVERTISE SERVICE: " << clientId << "," << srv_name;

	mtxSrvs.lock();
	if (srvs.find(srv_name) != srvs.end())
	{
		mtxSrvs.unlock();
		// | INS | ID | SUCCESS |
		auto resp = TCPMessage::Generate(
				INS_SIZE
				+ ID_SIZE
				+ sizeof(bool));

		resp->AppendData(std::uint8_t(Ins::ADVSRVACK));
		resp->AppendData(mid);
		resp->AppendData(false);

		conn->SendResponse(resp);
		LOG(INFO) << "NOT DONE.";
	}
	else
	{
		srvs.insert(std::pair<std::string, std::uint64_t>(srv_name, clientId));
		mtxSrvs.unlock();

		// | INS | ID | SUCCESS |
		auto resp = TCPMessage::Generate(
				INS_SIZE
				+ ID_SIZE
				+ sizeof(bool));

		resp->AppendData(std::uint8_t(Ins::ADVSRVACK));
		resp->AppendData(mid);
		resp->AppendData(true);

		conn->SendResponse(resp);

		LOG(INFO) << "DONE.";
	}
}

void Core::CallService(std::shared_ptr<TCPMessage> req, std::shared_ptr<light::TCPConnection> conn)
{
	// | INS* | ID | CLIENT_ID | SIZE_OF_SRV_NAME | SRV_NAME | SIZE_OF_DATA | DATA |
	std::uint64_t mid;
	std::string srv_name;
	std::uint64_t clientId;

	req->NextData(mid);
	req->NextData(clientId);
	req->NextData(srv_name);

	DLOG(INFO) << "CALL SERVICE: clientId:" << clientId
			<< ", messageId:" << mid
			<< ", srv_name:" << srv_name;
	mtxSrvs.lock();
	auto fd = srvs.find(srv_name);
	if (fd == srvs.end())
	{
		mtxSrvs.unlock();
		// | INS | ID | SUCCESS |
		auto resp = TCPMessage::Generate(
				INS_SIZE
				+ ID_SIZE
				+ sizeof(bool));

		resp->AppendData(std::uint8_t(Ins::SERVICE_RESPONSE));
		resp->AppendData(mid);
		resp->AppendData(false);

		conn->SendResponse(resp);
		return;
	}
	mtxSrvs.unlock();

	auto srv_conn =  nodes.at(nodeNames.at(fd->second));
	srv_conn->SendRequest(req);

//	// for test
//	// | INS | ID | SUCCESS |
//	auto resp = TCPMessage::Generate(
//			INS_SIZE
//			+ ID_SIZE
//			+ sizeof(bool));
//
//	resp->AppendData(std::uint8_t(Ins::SERVICE_RESPONSE));
//	resp->AppendData(mid);
//	resp->AppendData(true);
//
//	conn->SendResponse(resp);
}

void Core::ResponseService(std::shared_ptr<TCPMessage> req, std::shared_ptr<light::TCPConnection> conn)
{
	// | INS* | ID | CLIENT_ID | SUCCESS | SIZE_OF_DATA | DATA |
	std::uint64_t mid;
	std::uint64_t clientId;

	req->NextData(mid);
	req->NextData(clientId);

	DLOG(INFO) << "RESPONSE SERVICE: clientId:" << clientId
			<< ", messageId:" << mid;
	auto srv_conn =  nodes.at(nodeNames.at(clientId));

	srv_conn->SendResponse(req);
}

int Core::Start()
{
	DLOG(INFO) << "start service.";
	service->Start();
	return 0;
}

}


int main()
{
	light::Core core(20001);
	return core.Start();
}
