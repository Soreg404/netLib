#include "include.hpp"

net::TCPListener::TCPListener(serverFunc server, PORT port) {
	m_port = port;
	m_serverFunction = server;

	m_accepterThread = std::thread(&TCPListener::accepterWorker, this);
}

net::TCPListener::~TCPListener() {
	if(m_sockListener != INVALID_SOCKET) closesocket(m_sockListener);
	m_accepterThread.join();
}

void net::TCPListener::accepterWorker() {
	LOG("accepter thread starts");

	m_sockListener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(m_sockListener == INVALID_SOCKET) {
		LOG("error creating listener socket");
		return;
	}

	// create server binding
	SOCKADDR_IN srvIn{ 0 };
	srvIn.sin_family = AF_INET;
	srvIn.sin_port = htons(m_port);
	srvIn.sin_addr.s_addr = INADDR_ANY;
	//InetPtonA(AF_INET, "127.0.0.1", &srvIn.sin_addr.S_un.S_addr);

	if(bind(m_sockListener, reinterpret_cast<SOCKADDR *>(&srvIn), sizeof(srvIn)) == SOCKET_ERROR) {
		LOG("error binding listener socket");
		return;
	}

	if(listen(m_sockListener, 10) == SOCKET_ERROR) {
		LOG("error setting accepter socket to listen");
		return;
	}

	LOG("start listening on port %hu", m_port);
	while(true) {

		SOCKADDR_IN acceptedCon{ 0 };
		int SINSize = sizeof(acceptedCon);
		SOCKET tmp_sockAccept = accept(m_sockListener, reinterpret_cast<SOCKADDR *>(&acceptedCon), &SINSize);
		if(tmp_sockAccept == INVALID_SOCKET) {
			int err = WSAGetLastError();
			if(err != WSAEINTR) LOG("error accept: %i", err)
			else LOG("ending accept loop")
		}

		LOG("accepted %s:%hu", IP4ToStr(acceptedCon.sin_addr.s_addr).c_str(), ntohs(acceptedCon.sin_port));

		std::shared_ptr<Connection> con = std::make_shared<Connection>(tmp_sockAccept);

	}

	LOG("accepter thread quits");
}

void net::TCPListener::startServer(std::shared_ptr<net::Connection> con) {}
