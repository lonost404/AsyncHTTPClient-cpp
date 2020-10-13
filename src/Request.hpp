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
#include <typeinfo>
#include <variant>

#include "RequestLoop.hpp"
#include "json.hpp"

class Request {

    RequestLoop& rl;
    std::string method;
    std::string host;
    int content_length;
    char* buffer;
    bool ssl_connected;
    int last_buffer_size;
    std::string path;
    SSL* ssl;
    std::function<void(Request& req)> on_data;
    std::function<void(Request& req)> on_open;
    std::function<void(Request& req)> on_close;
    
    std::string request_buffer;
    std::map<std::string, std::string> request_headers;
    std::string request_data;

    int header_size;
   
    struct epoll_event event;
    char recv_buffer[1024];
    bool secure;
    

    void createSecureConnection();
    void recvData();
    
public:
    Request(RequestLoop& rl, std::string_view method, std::string_view url);
    ~Request();
    std::string headers;
    std::string text;

    int sockfd;
    int port;
    
    Request& onData(std::function<void(Request& req)> f);
    Request& onOpen(std::function<void(Request& req)> f);
    Request& onClose(std::function<void(Request& req)> f);
    
    void createConnection();
    void closeConnection();
    void buildRequest();
    void sendRequest();
    void end();
    Request& setData(std::string_view data, bool build = true);
    Request& setData(std::map<std::string, std::string> data, bool build = true);
    Request& setJsonData(nlohmann::json& data, bool build = true);
    Request& setHeaders(std::map<std::string, std::string> h, bool build = true);
    Request& setHeader(std::string_view key, std::string value, bool build = true);
    std::string_view getHeader(std::string_view key);
    std::string getHeaders();
    friend RequestLoop;
};

