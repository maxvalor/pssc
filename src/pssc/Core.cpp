/*
 * core.cpp
 *
 *  Created on: Mar 9, 2021
 *      Author: ubuntu
 */

#include "pssc/protocol/Core.h"
#include "pssc/protocol/Instruction.h"
#include <glog/logging.h>
#include <thread>
#include <string>
#include "pssc/protocol/msgs/pssc_msgs.h"
#include "pssc/util/Locker.h"

namespace pssc
{

Core::Core(int port)
{
	server = std::make_unique<TCPServer>(
			port,
			std::bind(&Core::OnConnected, this, std::placeholders::_1),
			std::bind(&Core::OnDisconnected, this, std::placeholders::_1)
	);
}

void Core::OnConnected(std::shared_ptr<TCPConnection> conn)
{
	DLOG(INFO) << "client connected.";

	conn->SetOnMessage(
		std::bind(&Core::DispatchMessage, this, conn, std::placeholders::_1)
	);

	conn->Start();

//				std::thread([this, conn]()
//				{
//					while (conn->IsRunning())
//					{
//						std::shared_ptr<TCPMessage> req;
//
//						if (!conn->R(req))
//						{
//							break;
//						}
//
//						std::uint8_t ins;
//						req->NextData(ins);
//
//						switch (ins)
//						{
//							case Ins::REGISTER:
//							{
//								Register(req, conn);
//								break;
//							}
//							/*case Ins::UNREGISTER:
//							{
//								std::string name((char*)req->body());
//								DLOG(INFO) << "UNREGISTER: " << name;
//
//								auto resp = TCPMessage::Generate(1);
//								resp->header.id = req->header.id;
//								resp->header.ins = Ins::UNREGISTER;
//								resp->header.type = TCPMessage::RESPONSE;
//								resp->header.bodyLength = 1;
//
//								if (nodes.find(name) == nodes.end())
//								{
//									nodes.erase(name);
//									DLOG(INFO) << "Unregister node " << name;
//									resp->body()[0] = 1;
//								}
//								else
//								{
//									DLOG(INFO) << "Not found node " << name;
//									resp->body()[0] = 0;
//								}
//
//								conn->SendResponse(resp);
//
//								break;
//							}
//
//							case Ins::SUBSCRIBE:
//							{
//								Subscribe(req, conn);
//								break;
//							}
//
//							case Ins::PUBLISH:
//							{
//								Publish(req, conn);
//								break;
//							}
//
//							case Ins::ADDVERTISE_SERVICE:
//							{
//								AdvertiseService(req, conn);
//								break;
//							}
//
//							case Ins::SERVICE_CALL:
//							{
//								CallService(req, conn);
//								break;
//							}
//
//							case Ins::SERVICE_RESPONSE:
//							{
//								ResponseService(req, conn);
//								break;
//							}
//
//							default:
//							{
//								DLOG(WARNING) << "UNKOWN";
//
//								auto resp = TCPMessage::Generate(1);
//								resp->header.id = req->header.id;
//								resp->header.ins = Ins::UNKOWN;
//								resp->header.type = TCPMessage::RESPONSE;
//								resp->body()[0] = false;
//								conn->SendResponse(resp);
//								break;
//							}
//							*/
//						}
//					}
//				}).detach();
}

void Core::OnDisconnected(std::shared_ptr<TCPConnection> conn)
{
	// remove name->connection
	for (auto & node : nodes)
	{
		if (node.second.get() == conn.get())
		{
			DLOG(INFO) << "node with id " << node.first << " was disconnected.";
			auto nodeId = node.first;
			nodes.erase(nodeId);
			return;
		}
	}
}

void Core::DispatchMessage(std::shared_ptr<TCPConnection> conn, std::shared_ptr<TCPMessage> msg)
{
	if (conn->IsRunning())
	{
		pssc_ins ins;
		msg->NextData(ins);

		switch (ins)
		{
			case Ins::REGISTER:
			{
				Register(conn, msg);
				break;
			}

			case Ins::PUBLISH:
			{
				Publish(conn, msg);
				break;
			}

			case Ins::SUBSCRIBE:
			{
				Subscribe(conn, msg);
				break;
			}
		}
	}
}

void Core::Register(std::shared_ptr<TCPConnection> conn, std::shared_ptr<TCPMessage> msg)
{
	DLOG(INFO) << "Register Received.";
	RegisterMessage req(msg);
	RegACKMessage ack;
	ack.messageId = req.messageId;

	pssc_write_guard guard(rwlckNodes);
	ack.nodeId = nodeIdGen.Next();
	if (nodes.find(ack.nodeId) != nodes.end())
	{
		ack.success = false;
	}
	else
	{
		nodes.insert(std::pair<pssc_id, std::shared_ptr<TCPConnection>>(ack.nodeId, conn));
		ack.success = true;
	}

	conn->PendMessage(ack.toTCPMessage());
	DLOG(INFO) << "Register Responsed with success:" << ack.success;
}


void Core::Publish(std::shared_ptr<TCPConnection> conn, std::shared_ptr<TCPMessage> msg)
{
	PublishMessage req(msg);
	DLOG(WARNING) << "PUBLISH: publisher id:" << req.publisherId;

	pssc_read_guard guardTopics(rwlckTopics);
	auto&& subscribers = topics.find(req.topic);
	if (subscribers != topics.end())
	{
		for (auto& subscriberId : subscribers->second)
		{
			if (subscriberId == req.publisherId && !req.feedback)
			{
				continue;
			}
			DLOG(WARNING) << "publish topic: " + req.topic + " to node with id: " << subscriberId;
			pssc_read_guard guardNodes(rwlckNodes);
			try {
				auto& subConn = nodes.at(subscriberId);
				DLOG(WARNING) << "publish data size: " << req.sizeOfData;
				subConn->PendMessage(msg);
			} catch (...) {
				// disconnected subscriber, do nothing
			}
		}
	}
}

void Core::Subscribe(std::shared_ptr<TCPConnection> conn, std::shared_ptr<TCPMessage> msg)
{
	SubscribeMessage req(msg);
	SubACKMessage resp;

	DLOG(INFO) << "SUBSCRIBE: " << req.subscriberId << "," << req.topic;

	pssc_write_guard guard(rwlckTopics);
	if (topics.find(req.topic) == topics.end())
	{
		std::list<pssc_id> subscribers;
		subscribers.emplace_back(req.subscriberId);
		topics.insert(std::pair<std::string, std::list<pssc_id>>(req.topic, subscribers));

		resp.success = true;
	}
	else
	{
		try {
			auto& subscribers = topics.at(req.topic);
			if (std::find(subscribers.begin(), subscribers.end(), req.subscriberId) == subscribers.end())
			{
				subscribers.push_back(req.subscriberId);
				DLOG(INFO) << "SUBSCRIBE: OK, count of subscriber:" << subscribers.size();
				resp.success = true;
			}
		}
		catch (...)
		{
			DLOG(ERROR) << "SUBSCRIBE: ???";
			resp.success = false;
		}
	}

	resp.messageId = req.messageId;
	conn->PendMessage(resp.toTCPMessage());
	DLOG(INFO) << "SUBSCRIBE response: " << req.subscriberId << "," << resp.success;
}



void Core::AdvertiseService(std::shared_ptr<TCPConnection> conn, std::shared_ptr<TCPMessage> msg)
{
	AdvertiseServiceMessage req(msg);
	AdvSrvACKMessage ack;
	ack.messageId = req.messageId;

	DLOG(INFO) << "ADVERTISE SERVICE: " << req.advertiserId << "," << req.srv_name;

	pssc_write_guard guard(rwlckSrvs);
	if (srvs.find(req.srv_name) != srvs.end())
	{
		ack.success = false;

		conn->PendMessage(ack.toTCPMessage());
		LOG(INFO) << "NOT DONE.";
	}
	else
	{
		srvs.insert(std::pair<std::string, pssc_id>(req.srv_name, req.advertiserId));
		ack.success = true;

		conn->PendMessage(ack.toTCPMessage());

		LOG(INFO) << "DONE.";
	}
}

/*
void Core::CallService(std::shared_ptr<TCPMessage> req, std::shared_ptr<TCPConnection> conn)
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

void Core::ResponseService(std::shared_ptr<TCPMessage> req, std::shared_ptr<TCPConnection> conn)
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
*/

int Core::Start()
{
	DLOG(INFO) << "start service.";
	server->Start();
	return 0;
}

}



