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


class Request;

class RequestLoop {
	std::map<std::string, addrinfo*> dns_cache;
	int epfd;
	int requests_num;
	bool stop;
	
public:	
	RequestLoop();
	void loop();
	Request& createRequest(std::string_view method, std::string_view url);
	friend Request;
	
};
