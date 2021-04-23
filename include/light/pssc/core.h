/*
 * core.h
 *
 *  Created on: Mar 9, 2021
 *      Author: ubuntu
 */

#ifndef LIGHT_PSSC_CORE_H_
#define LIGHT_PSSC_CORE_H_

#include "tcp/tcp_service.h"
#include "id_generator.h"

#include <unordered_map>
#include <list>

namespace light
{

class Core
{
public:
	Core(int port);
	int Start();
private:
	std::unique_ptr<TCPService> service;

	IDGenerator<std::uint64_t> nodeIdGen;

	std::unordered_map<std::string, std::shared_ptr<TCPConnection>> nodes;
	std::unordered_map<std::uint64_t, std::string> nodeNames;

	std::unordered_map<std::string, std::list<std::string>> topics;

	std::mutex mtxSrvs;
	std::unordered_map<std::string, std::uint64_t> srvs;

	void Publish(std::shared_ptr<TCPMessage> req, std::shared_ptr<light::TCPConnection> conn);
	void Subscribe(std::shared_ptr<TCPMessage> req, std::shared_ptr<light::TCPConnection> conn);
	void Register(std::shared_ptr<TCPMessage> req, std::shared_ptr<light::TCPConnection> conn);
	void AdvertiseService(std::shared_ptr<TCPMessage> req, std::shared_ptr<light::TCPConnection> conn);
	void CallService(std::shared_ptr<TCPMessage> req, std::shared_ptr<light::TCPConnection> conn);
	void ResponseService(std::shared_ptr<TCPMessage> req, std::shared_ptr<light::TCPConnection> conn);
};

};


#endif /* LIGHT_PSSC_CORE_H_ */
