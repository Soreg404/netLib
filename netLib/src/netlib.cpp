#include "netlib.h"

using namespace std::chrono_literals;

WSADATA wsa;

void net::init() {
	LOG("net init");
#ifdef _DEBUG
	APP_START_TIME = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
#endif

	if(WSAStartup(MAKEWORD(2, 2), &wsa) != NO_ERROR) {
		LOG("error initializing WSA");
	}

}

net::TCPListener::TCPListener() {
}

net::TCPListener::~TCPListener() {
	if(m_sockAccept != INVALID_SOCKET) closesocket(m_sockAccept);
	m_accepterThread.join();
}

int net::TCPListener::start(serverFunc server, USHORT port) {

	m_port = port;
	m_serverFunction = server;

	m_accepterThread = std::thread(&TCPListener::accepterWorker, this);


	return 1;
}

void net::TCPListener::accepterWorker() {
	LOG("accepter starts");

	m_sockAccept = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(m_sockAccept == INVALID_SOCKET) LOG("error creating listener socket");

	// create server binding
	SOCKADDR_IN srvIn{ 0 };
	srvIn.sin_family = AF_INET;
	srvIn.sin_port = htons(m_port);
	srvIn.sin_addr.s_addr = INADDR_ANY;
	//InetPtonA(AF_INET, "127.0.0.1", &srvIn.sin_addr.S_un.S_addr);

	if(bind(m_sockAccept, reinterpret_cast<SOCKADDR *>(&srvIn), sizeof(srvIn)) == SOCKET_ERROR) {
		LOG("error binding listener socket");
	}

	if(listen(m_sockAccept, 10) == SOCKET_ERROR) {
		LOG("error failed to listen");
	}

	LOG("start listener");
	while(true) {
		SOCKADDR_IN acceptedCon{ 0 };
		int SINSize = sizeof(acceptedCon);
		SOCKET tmp_sockAccept = accept(m_sockAccept, reinterpret_cast<SOCKADDR *>(&acceptedCon), &SINSize);
		if(tmp_sockAccept == INVALID_SOCKET) {
			int err = WSAGetLastError();
			if(err != WSAEINTR) LOG("error accept: %i", err)
			else LOG("ending accept loop")
			break;
		}



	}

	LOG("accepter quits");
}

