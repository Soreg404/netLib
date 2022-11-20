#include <netlib.h>

#include <iostream>

const char *req = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";

using namespace std::chrono_literals;

void conRecvWorker(net::Connection *con) {
	char buffer[256]{ 0 };

	con->startReceiving();

	while(*con) {
		con->awaitData(INFINITE);
		buffer[con->pullData(buffer, 255)] = 0;
		std::cout << (const char *)buffer;
	}

	LOG("con recv worker stop");
}

int main() {

	net::NetLib netlib;

	net::Connection con(net::strToIP4("127.0.0.1"), 80);
	
	std::thread conRecvThread(conRecvWorker, &con);

	con.pushData(req, strlen(req));

	std::this_thread::sleep_for(2s);
	LOG(" main -- kill connection");
	closesocket(con.m_sockConnected);

	conRecvThread.join();

	return 0;
}