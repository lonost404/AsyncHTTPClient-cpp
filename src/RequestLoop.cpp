
#include "Request.hpp"

RequestLoop::RequestLoop()
: epfd(epoll_create1(0)), stop(false), requests_num(0) { }


Request& RequestLoop::createRequest(std::string_view method, std::string_view url) { 
    return *(new Request(*this, std::move(method), std::move(url)));
}


void RequestLoop::loop() {
    struct epoll_event events[1024];
    int num_ready;
    
    struct epoll_event new_events;
    new_events.events = EPOLLIN;

    while (!stop) {
        num_ready = epoll_wait(epfd, events, 1024, 1000);

        if (num_ready < 0)
            perror("Error in epoll_wait!");

        for (int i = 0; i < num_ready; i++) {
            Request& r = *((Request*)events[i].data.ptr);

            if (events[i].events & (EPOLLIN)) {
                r.recvData();
            }

            if (events[i].events & EPOLLHUP) {
                printf("Error connecting to server, socket: %d\n", events[i].data.fd);
                requests_num--;
                epoll_ctl(epfd, EPOLL_CTL_DEL, r.sockfd, NULL);
                r.closeConnection();
            }

            if (events[i].events & EPOLLOUT) { // On connect
                //Modify epoll events
                new_events.data.ptr = &r;
                epoll_ctl(epfd, EPOLL_CTL_MOD, r.sockfd, &new_events);

                if (!r.secure) {
                    r.on_open(r);
                } else {
                    r.createSecureConnection();
                }
                
                
            }
        }
    }
}
