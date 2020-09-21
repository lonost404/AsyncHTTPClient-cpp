#include "Request.h"

int main(int argc, char **argv) {

	RequestLoop rl;

	rl.createRequest("GET", "https://g.co/").onOpen([] (Request& req) {
		req.sendRequest();
	}).onData([] (Request& req) {
		std::cout << "Headers: " << req.headers << std::endl;
		std::cout << "Data: " << req.text << std::endl;
		req.closeConnection();
		req.end();
	}).onClose([] (Request& req) {
		std::cout << "Closing connection.." << std::endl;
	});

	rl.loop();
}
