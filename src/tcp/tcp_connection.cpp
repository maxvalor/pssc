/*
 * TCPConnection.h
 *
 *    Created on: Mar 8, 2021
 *            Author: ubuntu
 */


#include <glog/logging.h>
#include <light/tcp/tcp_connection.h>

namespace light {


TCPConnection::TCPConnection(tcp::socket socket, std::function<void(std::shared_ptr<light::TCPConnection>)> on_disconnected)
				: sock(std::move(socket)), OnDisconnected(on_disconnected)
{
	running = true;
    recvBuffer = TCPMessage::Generate();
}


TCPConnection::~TCPConnection() {}

bool TCPConnection::SendRequest(std::shared_ptr<TCPMessage> msg)
{
	std::lock_guard<std::mutex> lck(mtxSend);
	msg->header.type = TCPMessage::REQUEST;
	DLOG(INFO) << "SendRequest";
	Write(msg);
	return true;
}

bool TCPConnection::SendResponse(std::shared_ptr<TCPMessage> msg)
{
	std::lock_guard<std::mutex> lck(mtxSend);
	msg->header.type = TCPMessage::RESPONSE;
	DLOG(INFO) << "SendResponse";
	Write(msg);
	return true;
}

bool TCPConnection::WaitForRequest(std::shared_ptr<TCPMessage>& req)
{
	DLOG(INFO) << "WaitForRequest";

	std::unique_lock<std::mutex>  lck(mtxRecvReq);
	cvRecvReq.wait(lck, [this]()
	{
		return !requests.empty();
	});


	req = *requests.begin();
	requests.pop_front();

	DLOG(INFO) << "Request Get";

	return true;
}

bool TCPConnection::WaitForResponse(std::shared_ptr<TCPMessage>& resp)
{
	DLOG(INFO) << "WaitForResponse";

	std::unique_lock<std::mutex>  lck(mtxRecvResp);
	cvRecvResp.wait(lck, [this]()
	{
		return !responses.empty();
	});


	resp = *responses.begin();
	responses.pop_front();

	DLOG(INFO) << "Response Get";

	return true;
}

void TCPConnection::Start()
{
	ReadHeader();
}

void TCPConnection::ReadHeader()
{
	DLOG(INFO) << "ReadHeader";
	auto self = shared_from_this();
    boost::asio::async_read(
		    sock, boost::asio::buffer(&recvBuffer->header, sizeof(TCPMessage::Header)),
            [this, self](boost::system::error_code ec, std::size_t length)
	{
		DLOG(INFO) << "Read header from remote, size: " << length;
		if (!ec && recvBuffer->header.decode() && recvBuffer->header.bodyLength > 0)
		{
			DLOG(INFO) << "bodyLength:" << recvBuffer->header.bodyLength;
			ReadBody();
		}
		else
		{
			DLOG(WARNING) << "bodyLength:" << recvBuffer->header.bodyLength;
			if (ec)
			{
				DLOG(ERROR) << "ReadHeader error: " << ec.message() << "\t" << ec;
				if (OnDisconnected != nullptr)
				{
					running = false;
					sock.close();
					cvRecvReq.notify_one();
					cvRecvResp.notify_one();
					OnDisconnected(self);
				}
			}
		}
	});
}

void TCPConnection::ReadBody() {

	DLOG(INFO) << "ReadBody";
    auto self = shared_from_this();

    auto rbuf = TCPMessage::Generate(recvBuffer->header);

    boost::asio::async_read(
		    sock, boost::asio::buffer(rbuf->body(),rbuf->header.bodyLength),
			[this, self, rbuf](boost::system::error_code ec, std::size_t size)
    {
    	DLOG(INFO) << "recv body size:" << size;
		if (!ec && rbuf->header.bodyLength)
		{
			try
			{
				switch (rbuf->header.type)
				{
					case TCPMessage::REQUEST:
					{
						DLOG(INFO) << "Connection Request.";
						mtxRecvReq.lock();
						requests.push_back(rbuf);
						mtxRecvReq.unlock();

						cvRecvReq.notify_one();
						break;
					}
					case TCPMessage::RESPONSE:
					{
						DLOG(INFO) << "Connection Response.";
						mtxRecvResp.lock();
						responses.push_back(rbuf);
						mtxRecvResp.unlock();

						cvRecvResp.notify_one();
						break;
					}
				}
			} catch (...) {
				DLOG(ERROR) << "an exception occurred after receive message.";
			}
		}
		else
		{
			if (OnDisconnected != nullptr)
			{
				running = false;
				sock.close();
				cvRecvReq.notify_one();
				cvRecvResp.notify_one();
				OnDisconnected(self);
			}
		}
		ReadHeader();
    });

}

bool TCPConnection::Write(std::uint8_t* data, size_t size)
{
	boost::system::error_code ec;
	std::uint32_t infactSend = 0;
	std::uint32_t restSend = size;

	// send message
	do {
		std::size_t _infactSend = sock.write_some(
			boost::asio::buffer(data + infactSend, restSend), ec);
		if (ec)
		{
			break;
		}
		infactSend = infactSend + _infactSend;
		restSend = restSend - _infactSend;
	} while (restSend > 0 && running);

	if (ec) {
		running = false;
		sock.close();
		cvRecvReq.notify_one();
		cvRecvResp.notify_one();
		OnDisconnected(shared_from_this());
		DLOG(ERROR) << "a fatal error occurs while sending data, system exit.";
		return false;
	}
	return true;
}

bool TCPConnection::Write(std::shared_ptr<TCPMessage> resp)
{
    auto bodyLength = resp->header.bodyLength;
    // encode
    resp->header.encode();

    DLOG(INFO) << "data size:" << bodyLength;

    bool rlt = false;

    if(Write((std::uint8_t*)&resp->header, sizeof(TCPMessage::Header)))
    {
    	rlt = Write(resp->body(), bodyLength);
    }

    resp->header.decode();

    return rlt;
}

}


