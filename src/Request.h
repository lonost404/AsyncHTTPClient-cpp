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

    char* buffer;
    int header_size;
    int content_length;
    bool ssl_connected;
    int last_buffer_size;
    char * method;
    struct epoll_event event;
    char recv_buffer[1024];
    bool secure;
    std::function<void(Request& req)> on_data;
    std::function<void(Request& req)> on_open;
    std::function<void(Request& req)> on_close;
    
    SSL* ssl;
    RequestLoop& rl;
    std::string path;

    void createSecureConnection();
    void recvData();
    


public:
    const char* data;
    const char* text;
    int sockfd;
    int port;
    char* headers;
    std::string host;

    Request& onData(std::function<void(Request& req)> f);
    Request& onOpen(std::function<void(Request& req)> f);
    Request& onClose(std::function<void(Request& req)> f);
    
    void createConnection();
    void closeConnection();
    void sendRequest();
    void end();

    Request(RequestLoop& rl, const char *method, const char *url, const char *data = nullptr);
    ~Request();
    friend RequestLoop;
};

