#include <netlib.h>

#include <iostream>

const char *req = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";

using namespace std::chrono_literals;

net::serverFunc server;

int main() {

	net::NetLib netlib;

	net::TCPListener l(server, 8080);

	std::this_thread::sleep_for(10s);

	return 0;
}

int server(net::CON_PTR con) {

	con->pushData("hello world!", 12);



	return 0;
}