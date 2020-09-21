#include "Request.h"



Request::Request(RequestLoop& rl, const char *method, const char *url, const char *data) 
: rl(rl), 
  method((char*)method), 
  data(data), 
  host(url),
  content_length(0x7FFFFFFF),
  buffer(nullptr),
  ssl_connected(false),
  last_buffer_size(0),
  path("/"),
  ssl(nullptr),
  // Default functions 
  on_data([] (Request& r) { r.closeConnection(); r.end(); }),
  on_open([] (Request& r) { r.sendRequest(); }), 
  on_close([] (Request& r) { }),
  headers(nullptr) {
    
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

    createConnection();
}

Request::~Request() {
    free(buffer);
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

        if (!headers) {

            char *p = strstr(recv_buffer, "\r\n\r\n");

            if (p) { // Headers are ready

                int size = last_buffer_size + (p - recv_buffer);
                headers = (char *)malloc(size);
                memcpy(headers, recv_buffer, size);

                //Set header size
                header_size = size;

                //Get content-length
                char *pos = strstr(headers, "Content-Length: ") + 16;
                content_length = atoi(pos);

            }
        }
        
        last_buffer_size += recv_size;

        if ((last_buffer_size - header_size - 4) >= content_length) { // All request recived
            text = buffer + header_size + 4;
            on_data(*this);            
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
    --rl.connections;
    on_close(*this);
    
}
void Request::end() {
    
    if (!rl.connections) {
        rl.stop = true;
    }
    delete this;
}
void Request::createConnection() {
    rl.connections++;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    auto pos = rl.dns_cache.find(host);
    struct addrinfo *res = (addrinfo *)malloc(sizeof(addrinfo));

    if (pos == rl.dns_cache.end()) {
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
        rl.dns_cache.insert(std::make_pair(host, *res));
    }
    else { 
        *res = pos->second;
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

void Request::sendRequest() {

    if (buffer) {
        free(buffer);
        free(headers);

        buffer = nullptr;
        headers = nullptr;

        content_length = 0;
        header_size = 0;
        last_buffer_size = 0;   
    }
    std::string _data;
    if (data == nullptr) {
        _data = std::string(method) + " " + path + " HTTP/1.1\r\nHost: " + host + "\r\n\r\n";
    }
    else {
        _data = std::string(method) + " " + path + " HTTP/1.1\r\nHost: " + host + "\r\n" + data + "\r\n";
    }

    if (secure) {
        SSL_write(ssl, _data.c_str(), _data.length());
    } else {
        send(sockfd, _data.c_str(), _data.length(), 0);
    }
}
void Request::createSecureConnection() {
    if (!ssl) {
        const SSL_METHOD *meth = TLSv1_2_client_method();
        SSL_CTX *ctx = SSL_CTX_new(meth);
        ssl = SSL_new(ctx);

        if (!ssl)
        {
            std::cout << "Error creating SSL.\n"
                    << std::endl;
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
