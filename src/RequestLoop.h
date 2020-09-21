#pragma once
#include <iostream>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <fcntl.h>
#include <strings.h>
#include <unistd.h>
#include <string>
#include <map>

#include "Request.h"

class Request;

class RequestLoop {
	std::map<std::string, struct addrinfo> dns_cache;
	int epfd;
	int connections;
	bool stop;
	
public:	
	RequestLoop();
	void loop();
	Request& createRequest(const char* method, const char* url);

	friend Request;
	
};
