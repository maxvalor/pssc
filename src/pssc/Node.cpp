/*
 * Node.cpp
 *
 *  Created on: Apr 25, 2021
 *      Author: ubuntu
 */

#include "pssc/protocol/Node.h"
#include "pssc/protocol/types.h"

#include "pssc/protocol/msgs/pssc_msgs.h"

namespace pssc {

bool Node::Initialize(int port)
{
	client = std::make_shared<TCPClient>(
		port,
		std::bind(&Node::OnConntected, this, std::placeholders::_1),
		std::bind(&Node::OnDisconntected, this, std::placeholders::_1)
	);

	client->Connect();

	return startNoti.wait_for(std::chrono::milliseconds(300)) == std::cv_status::no_timeout;
}

void Node::OnConntected(std::shared_ptr<TCPConnection> conn)
{
	this->conn = conn;
	DLOG(INFO) << "connected.";

	conn->SetOnMessage(std::bind(&Node::DispatchMessage, this, std::placeholders::_1));

	RegisterMessage req;
	req.messageId = messageIdGen.Next();
	conn->PendMessage(req.toTCPMessage());
}

void Node::DispatchMessage(std::shared_ptr<TCPMessage> msg)
{
	pssc_ins ins;
	msg->NextData(ins);
	switch(ins)
	{
		case Ins::REGACK:
		{
			RegACKMessage ack(msg);
			if (ack.success)
			{
				LOG(INFO) << "Success to register node with id " << ack.nodeId;
				nodeId = ack.nodeId;
				startNoti.notify_one();
			}
			else
			{
				LOG(WARNING) << "Failed to register node.";
			}
			break;
		}

		case Ins::PUBLISH:
		{
			OnPublish(msg);
			break;
		}

		case Ins::SUBACK:
		{
			pssc_id messageId;
			msg->NextData(messageId);
			msg->Reset();
			msg->IgnoreBytes(SIZE_OF_PSSC_INS);

			mtxAcks.lock();
			acks.insert(std::pair<pssc_id, std::shared_ptr<TCPMessage>>(messageId, msg));
			auto funcNoti = mapAckNoti.at(messageId);
			mapAckNoti.erase(messageId);
			mtxAcks.unlock();

			funcNoti();
			break;
		}

		case Ins::ADVSRVACK:
		{
			pssc_id messageId;
			msg->NextData(messageId);
			msg->Reset();
			msg->IgnoreBytes(SIZE_OF_PSSC_INS);

			mtxAcks.lock();
			acks.insert(std::pair<pssc_id, std::shared_ptr<TCPMessage>>(messageId, msg));
			auto funcNoti = mapAckNoti.at(messageId);
			mapAckNoti.erase(messageId);
			mtxAcks.unlock();

			funcNoti();
			break;
		}
	}
}

void Node::OnPublish(std::shared_ptr<TCPMessage> msg)
{
	LOG(INFO) << "Received Publish.";
	PublishMessage req(msg);
	topicCallback(req.topic, req.data, req.sizeOfData);
}

void Node::OnDisconntected(std::shared_ptr<TCPConnection> conn)
{
	running = false;
	DLOG(INFO) << "disconnected.";
}

bool Node::SendRequestAndWaitForResponse(pssc_id messageId, std::shared_ptr<TCPMessage> req, std::shared_ptr<TCPMessage>& resp)
{
	auto noti = std::make_shared<util::Notifier>();
	auto f = [noti]()
	{
		noti->notify_one();
	};

	mtxAcks.lock();
	mapAckNoti.insert(std::pair<pssc_id, std::function<void()>>(messageId, f));
	mtxAcks.unlock();
	conn->PendMessage(req);

	auto rlt = noti->wait_for(std::chrono::milliseconds(3000));
	if (rlt == std::cv_status::timeout)
	{
		return false;
	}
	std::lock_guard<std::mutex> lck(mtxAcks);
	resp = acks.at(messageId);
	return true;
}

void Node::Publish(std::string topic, std::uint8_t*data, size_t size, bool feedback)
{
	PublishMessage req;
	req.messageId = messageIdGen.Next();
	req.publisherId = nodeId;
	req.topic = topic;
	req.sizeOfData = size;
	req.data = data;
	req.feedback = feedback;

	conn->PendMessage(req.toTCPMessage());
}


bool Node::Subscribe(std::string topic)
{
	SubscribeMessage req;
	req.messageId = messageIdGen.Next();
	req.subscriberId = nodeId;
	req.topic = topic;

	std::shared_ptr<TCPMessage> msg;
	if(!SendRequestAndWaitForResponse(req.messageId, req.toTCPMessage(), msg))
	{
		return false;
	}

	SubACKMessage resp(msg);
	return resp.success;
}

bool Node::AdvertiseService(std::string srv_name)
{
	AdvertiseServiceMessage req;
	req.messageId = messageIdGen.Next();
	req.advertiserId = nodeId;
	req.srv_name = srv_name;

	std::shared_ptr<TCPMessage> msg;
	if(!SendRequestAndWaitForResponse(req.messageId, req.toTCPMessage(), msg))
	{
		return false;
	}

	AdvSrvACKMessage resp(msg);
	return resp.success;
}

}


