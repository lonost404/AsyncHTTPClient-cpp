#include "Request.hpp"

int main(int argc, char **argv) {

	RequestLoop rl;

	for (int i=0; i<2; i++) {
		rl.createRequest("POST", "https://g.co/").setData({{"key1", "value1"}, {"key2", "value2"}}).onData([] (auto& req) {
			std::cout<<req.text<<std::endl;
			req.closeConnection();
		});
	}
	

	rl.loop();
}