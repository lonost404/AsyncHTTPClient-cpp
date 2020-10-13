#include "Request.hpp"

Request::Request(RequestLoop& rl, std::string_view method, std::string_view url) 
: rl(rl), 
  method(std::move(method)), 
  host(std::move(url)),
  content_length(0x7FFFFFFF),
  buffer(nullptr),
  ssl_connected(false),
  last_buffer_size(0),
  path("/"),
  ssl(nullptr),
  on_data([] (Request& r) { r.closeConnection(); }),
  on_open([] (Request& r) { r.sendRequest(); }), 
  on_close([] (Request& r) { r.end(); }),
  request_buffer(""),
  request_headers({ }), 
  header_size(0) { 
    
    rl.requests_num++;

    if (host.find("http://") == 0) {
        host = host.substr(7, host.length());
        port = 80;        
    }
    else if (host.find("https://") == 0) {
        secure = true;
        host = host.substr(8, host.length());
        port = 443;
    } else {
        printf("Error, invalid protocol\n");
        return;
    }

    if (auto pos = host.find("/"); pos != std::string::npos) {
        path = host.substr(pos, host.length());
        host = host.substr(0, pos);
    }

    if (auto pos = host.find(":"); pos != std::string::npos) {
        port = atoi(host.c_str() + pos + 1);
        host = host.substr(0, pos);
    }

    request_headers = {{"Host", host}, {"User-Agent", "AsyncHttpClient"}, {"Accept", "*/*"}}; // Default headers

    buildRequest();
    createConnection();
}

void Request::recvData() {
    int recv_size;
    char * ssl_buffer = nullptr;
    if (secure) {
        if (ssl_connected) {
            recv_size = SSL_read(ssl, recv_buffer, 1024);
            if (int pending = SSL_pending(ssl); pending > 0) {
                char *new_buffer = (char*)malloc(pending + recv_size);
                
                memcpy(new_buffer, recv_buffer, recv_size);

                recv_size += SSL_read(ssl, new_buffer + recv_size, pending);
                ssl_buffer = new_buffer;
            }
        }
        else {
            createSecureConnection();
            return;
        }
    }
    else {
        recv_size = recv(sockfd, recv_buffer, 1024, 0);
    }

    if (recv_size > 0) {
        char *new_buffer;

        new_buffer = (char *)malloc(last_buffer_size + recv_size + 1);
        new_buffer[last_buffer_size + recv_size] = '\0';

        memcpy(new_buffer, buffer, last_buffer_size);

        //Copy new data
        if (ssl_buffer) {
            memcpy(new_buffer + last_buffer_size, ssl_buffer, recv_size);
            free(ssl_buffer);
        } else {
            memcpy(new_buffer + last_buffer_size, recv_buffer, recv_size);
        }
        free(buffer);
        //Set new buffer
        buffer = new_buffer;

        if (headers.empty()) {

            char *p = strstr(recv_buffer, "\r\n\r\n");

            if (p) { // Headers are ready

                int size = last_buffer_size + (p - recv_buffer);
                char h[size]; 
                memcpy(h, recv_buffer, size);
                headers = h;

                //Set header size
                header_size = size;

                //Get content-length
                const char *pos = strstr(headers.c_str(), "Content-Length: ") + 16;
                content_length = atoi(pos);

            }
        }
        
        last_buffer_size += recv_size;

        if ((last_buffer_size - header_size - 4) >= content_length) { // All request recived
            text = std::string((char*)buffer + header_size + 4);

            on_data(*this);

            content_length = 0;
            header_size = 0;
            last_buffer_size = 0;

            headers.erase();
            
        }
    }
    else if (!recv_size) {
        closeConnection();
    }
}

Request& Request::onData(std::function<void(Request& req)> f) {
	on_data = f;
	return *this;
}

Request& Request::onClose(std::function<void(Request& req)> f) {
	on_close = f;
	return *this;
}
Request& Request::onOpen(std::function<void(Request& req)> f) {
    on_open = f;
    return *this;
}

void Request::closeConnection() {
    epoll_ctl(rl.epfd, EPOLL_CTL_DEL, sockfd, NULL);
    if (secure) { 
        SSL_shutdown(ssl); 
        ssl = nullptr;
    }

    ssl_connected = false;
    close(sockfd);
    on_close(*this);
    
}

Request::~Request() {
    if (buffer) { free(buffer); }
}

void Request::end() {
    if (--rl.requests_num <= 0) {
        rl.stop = true;
    }

    delete this;
}
void Request::createConnection() {
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    auto pos = rl.dns_cache.find(host);
    struct addrinfo *res;
    

    if (pos == rl.dns_cache.end()) {
        //res = (addrinfo *)malloc(sizeof(addrinfo));
        //Resolve hostname
        struct addrinfo hints;

        bzero(&hints, sizeof(hints));
        hints.ai_family = PF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags |= AI_CANONNAME;

        if (getaddrinfo(host.c_str(), NULL, &hints, &res) != 0) {
            perror("getaddrinfo");
            printf("Error in name resolution\n");
            return;
        }

        //Insert in dns cache table
        rl.dns_cache.insert(std::make_pair(host, res));
    }
    else { 
        res = pos->second;
    }
    

    //Set nonblocking socket
    fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK);

    //Create sockaddr
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;

    //Set events
    event.events = EPOLLIN | EPOLLOUT;
    
    //Set socket to event
    event.data.ptr = this;

    //Set epoll
    epoll_ctl(rl.epfd, EPOLL_CTL_ADD, sockfd, &event);

    //Connect to host
    connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    
}
void Request::buildRequest() {
    
    /*if (buffer) {
        free(buffer);
        free(headers);

        buffer = nullptr;
        headers = nullptr;

        content_length = 0;
        header_size = 0;
        last_buffer_size = 0;
    }*/

    request_buffer = method + " " + path + " HTTP/1.1\r\n";
    for(auto &header : request_headers) {
        request_buffer += header.first + ": " + header.second + "\r\n";
    }

    request_buffer +=  "\r\n" + request_data;
}

void Request::sendRequest() {
    if (secure) {
        SSL_write(ssl, request_buffer.c_str(), request_buffer.length());
    } else {
        send(sockfd, request_buffer.c_str(), request_buffer.length(), 0);
    }
}
void Request::createSecureConnection() {
    if (!ssl) {
        const SSL_METHOD *meth = TLSv1_2_client_method();
        SSL_CTX *ctx = SSL_CTX_new(meth);
        ssl = SSL_new(ctx);

        if (!ssl)
        {
            printf("Error creating SSL.\n");
            return;
        }
        
        SSL_set_fd(ssl, sockfd);
    }
    if (SSL_connect(ssl) >= 0) {
        ssl_connected = true;
        on_open(*this);
    } else {
        ssl_connected = false;
    }

}

Request& Request::setData(std::string_view data, bool build) {
    request_data = data;
    request_headers["Content-Length"] = std::to_string(request_data.length());
    if (build) { 
        buildRequest(); 
    }
    return *this;
}

Request& Request::setData(std::map<std::string, std::string> data, bool build) {
    
    request_data = "";
    for(auto &data_pair : data) {
        request_data += data_pair.first + "=" + data_pair.second + "&";
    }

    request_data = request_data.substr(0, request_data.length()-1);
    request_headers["Content-Length"] = std::to_string(request_data.length());
    if (build) {
        buildRequest(); 
    }
    return *this;
}


Request& Request::setJsonData(nlohmann::json& data, bool build) {
    
    request_data = data.dump();

    request_headers["Content-Length"] = std::to_string(request_data.length());
    request_headers["Content-Type"] = "application/json";
    
    if (build) {
        buildRequest(); 
    }
    return *this;
}

Request& Request::setHeader(std::string_view key, std::string value, bool build) {
    request_headers[key.data()] = value;
    if (build) { 
        buildRequest(); 
    }
    return *this;
}

Request& Request::setHeaders(std::map<std::string, std::string> h, bool build) {
    request_headers = std::move(h);
    if (build) { 
        buildRequest(); 
    }
    return *this;
}

std::string_view Request::getHeader(std::string_view key) {
    std::string_view value;

    if (auto pos = headers.find(key); pos != std::string::npos) {
        value = headers.substr(pos + key.size() + 2, headers.length()).c_str();
        value = value.substr(0, value.find("\r"));
    } else {
        value = "";
    }
    return value;
}
std::string Request::getHeaders() {
    return headers;
}