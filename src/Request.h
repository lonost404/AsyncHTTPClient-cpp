#pragma once
#include <iostream>
#include <string.h>
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
#include <vector>
#include <functional>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <thread>

#include "RequestLoop.h"

class Request {

    char* buffer = nullptr;
    int header_size;
    int content_length = 0x7FFFFFFF;
    bool ssl_connected = false;
    int last_buffer_size = 0;
    char * method;
    struct epoll_event event;
    char recv_buffer[1024];
    bool secure = false;
    std::function<void(Request& req)> on_data;
    std::function<void(Request& req)> on_close;
    SSL* ssl = nullptr;
    RequestLoop& rl;
    std::string path = "/";

    void createSecureConnection();
    
    void recvData();
    


public:
    char* data;
    char* text;
    int sockfd;
    int port;
    char* headers = nullptr;
    std::string host;

    Request& onData(std::function<void(Request& req)> f);
    Request& onClose(std::function<void(Request& req)> f);
    void createConnection();
    void closeConnection();
    void sendRequest();
    void end();

    Request(RequestLoop& rl, const char *method, const char *url, const char *data = nullptr);
    ~Request();
    friend RequestLoop;
};

