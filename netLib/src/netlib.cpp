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

byte net::IP4Octet(IP4 addr, unsigned int octet) {
	return --octet > 3 ? 0 : ((addr >> (8 * octet)) & 0xff);
}

net::IP4 net::strToIP4(const char *str) {
	IP4 addr = 0;
	InetPtonA(AF_INET, str, &addr);
	return addr;
}

std::string net::IP4ToStr(IP4 addr) {
	char buff[32]{ 0 };
	int offs = 0;
	for(int i = 1; i <= 4; i++) {
		int w = sprintf_s(buff + offs, 4, "%i", IP4Octet(addr, i));
		if(w < 0) return "0.0.0.0";
		offs += w;
		buff[offs++] = i == 4 ? 0 : '.';
	}
	return buff;
}

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

		std::shared_ptr<Connection> con = std::make_shared<Connection>(tmp_sockAccept, srvIn, acceptedCon);

	}

	LOG("accepter thread quits");
}


void net::TCPListener::startServer(std::shared_ptr<net::Connection> con) {
}

net::Connection::Connection(IP4 addr, PORT port) {
	m_sockConnected = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	SOCKADDR_IN peerInfo;
	peerInfo.sin_family = AF_INET;
	peerInfo.sin_addr.s_addr = addr;
	peerInfo.sin_port = htons(port);


	int iResult = connect(m_sockConnected, reinterpret_cast<SOCKADDR *>(&peerInfo), sizeof(peerInfo));
	if(iResult == SOCKET_ERROR) {
		LOG("connect function failed with error: %ld\n", WSAGetLastError());
		iResult = closesocket(m_sockConnected);
		if(iResult == SOCKET_ERROR)
			LOG("closesocket function failed with error: %ld\n", WSAGetLastError());
		return;
	}

	SOCKADDR_IN selfName;
	int namelen = sizeof(selfName);
	getsockname(m_sockConnected, reinterpret_cast<SOCKADDR *>(&selfName), &namelen);

	m_addrPeer = addr;
	m_portPeer = peerInfo.sin_port;
	m_addrSelf = selfName.sin_addr.s_addr;
	m_portSelf = selfName.sin_port;

	m_connected = true;

}

net::Connection::Connection(SOCKET s, SOCKADDR_IN self, SOCKADDR_IN peer) {
	m_sockConnected = s;
	m_addrPeer = peer.sin_addr.s_addr;
	m_portPeer = peer.sin_port;
	m_addrSelf = self.sin_addr.s_addr;
	m_portSelf = self.sin_port;

	m_connected = true;
}

net::Connection::~Connection() {}


void net::Connection::startReceiving() {}

