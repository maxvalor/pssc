/*
 * TCPMessage.h
 *
 *    Created on: Mar 8, 2021
 *            Author: ubuntu
 */

#ifndef LIGHT_TCP_TCP_MESSAGE_H_
#define LIGHT_TCP_TCP_MESSAGE_H_

#include <memory>
#include <string>
#include <string.h>
#include <iostream>
#include <netinet/in.h>

namespace light {

class TCPMessage : public std::enable_shared_from_this<TCPMessage>
{
public:

	TCPMessage()
	{
		memset(&header, 0x00, sizeof(header));
		_body = nullptr;
		release = true;
		offset = 0;
	}

	enum {
		REQUEST,
		RESPONSE
	};


	virtual ~TCPMessage()
	{
		if (release && _body != nullptr)
		{
			delete[] _body;
		}
	}

    struct Header
    {
        std::uint64_t bodyLength;
        std::uint32_t type;

        bool decode()
        {
            bodyLength = ntohl(bodyLength);
            return bodyLength > 0;
        }

        bool encode()
        {
        	if (bodyLength <= 0)
        	{
        		return false;
        	}
            bodyLength = htonl(bodyLength);
            return true;
        }
    }header;


    static std::shared_ptr<TCPMessage> Generate(size_t size = 0)
    {
    	auto msg = std::make_shared<TCPMessage>();
    	if (size > 0)
    	{
    		msg->_body = new std::uint8_t[size];
    	}
        msg->header.bodyLength = size;
        return msg;
    }

    static std::shared_ptr<TCPMessage> Generate(Header header, std::uint8_t* data = nullptr)
    {
    	auto msg = std::make_shared<TCPMessage>();
    	if (data == nullptr)
    	{
        	if (header.bodyLength > 0)
        	{
        		msg->_body = new std::uint8_t[header.bodyLength];
        	}
    	}
    	else
    	{
            msg->_body = data;
            msg->release = false;
    	}
    	msg->header = header;
        return msg;
    }

    inline std::uint64_t size()
    {
        return sizeof(Header) + header.bodyLength;
    }

    inline std::uint8_t* body()
	{
    	return _body;
	}

    inline std::uint8_t* cur()
    {
    	return _body + offset;
    }

    void Reset()
    {
    	offset = 0;
    }

    void IgnoreData(size_t size)
    {
    	offset += size;
    }

    template <typename T>
    bool AppendData(T data)
    {
    	auto&& size = sizeof(T);
    	if (offset + size <= header.bodyLength)
    	{
        	memcpy(_body + offset, &data, size);
        	offset += size;
        	return true;
    	}
    	return false;
    }

    bool AppendData(std::string data)
    {
    	auto size = data.size();
    	if (offset + size + sizeof(size_t) <= header.bodyLength)
    	{
    		memcpy(_body + offset, &size, sizeof(size_t));
    		offset += sizeof(size_t);
        	memcpy(_body + offset, data.c_str(), size);
        	offset += data.size();
        	return true;
    	}
    	return false;
    }

    bool AppendData(std::uint8_t* data, size_t size)
    {
    	if (offset + size <= header.bodyLength)
    	{
    		memcpy(_body + offset, data, size);
        	offset += size;
        	return true;
    	}
    	return false;
    }

    template <typename T>
    void NextData(T& data)
    {
    	memcpy(&data, _body + offset, sizeof(T));
    	offset += sizeof(T);
    }

    void NextData(std::string& data)
    {
    	size_t size;
    	memcpy(&size, _body + offset, sizeof(size_t));
    	offset += sizeof(size_t);
    	auto temp = new char[size + 1];
    	memcpy(temp, _body + offset, size);
    	offset += size;
    	temp[size] = '\0';

    	std::string str(temp);
    	delete[] temp;
    	data = str;
    }

    void NextData(std::uint8_t* data, size_t size)
    {
    	memcpy(data, _body + offset, size);
    	offset += size;
    }

private:

    std::uint8_t* _body;
    size_t offset;
    bool release;
};

}

#endif /* LIGHT_TCP_TCP_MESSAGE_H_ */
