# AsyncHTTPClient-cpp
This is my simple Asynchronous HTTP client using epoll events
## Simple usage
```c++
#include "Request.h"

int main() {

    RequestLoop rl;

    rl.createRequest("GET", "https://g.co/").onData([] (Request& req) {
        std::cout << "Headers: " << req.headers << std::endl;
        std::cout << "Data: " << req.text << std::endl;
        req.closeConnection();
    });
    
    rl.loop();
    
    return 0;

}

```
