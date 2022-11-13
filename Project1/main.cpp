#include <netlib.h>

#include <iostream>

int main() {

	net::init();

	net::TCPListener listenner;

	listenner.start(nullptr, 3570);

	return 0;
}