#include "netlib.h"

#ifdef _DEBUG
std::mutex LOG_MUTEX;
#endif

using namespace std::chrono_literals;

WSADATA wsa;
bool WSAINIT = false;

net::NetLib::NetLib() {

	if(WSAINIT) return;

#ifdef _DEBUG
	APP_START_TIME = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
#endif

	LOG("WSA startup");
	if(WSAStartup(MAKEWORD(2, 2), &wsa) != NO_ERROR) {
		LOG("error initializing WSA");
	} else WSAINIT = true;

}

net::NetLib::~NetLib() {
	if(WSAINIT) {
		LOG("WSA cleanup");
		WSACleanup();
	}
}

byte net::IP4Octet(IP4 addr, unsigned int octet) {
	return --octet > 3 ? 0 : reinterpret_cast<byte *>(&addr)[octet];
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

		std::shared_ptr<Connection> con = std::make_shared<Connection>(tmp_sockAccept);

	}

	LOG("accepter thread quits");
}


void net::TCPListener::startServer(std::shared_ptr<net::Connection> con) {
}

net::Connection::Connection(IP4 addr, PORT port) {

	initEvents();

	m_sockConnected = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	LOG("connecting to %s:%hu", IP4ToStr(addr).c_str(), port);

	port = htons(port);

	SOCKADDR_IN peerInfo;
	peerInfo.sin_family = AF_INET;
	peerInfo.sin_addr.s_addr = addr;
	peerInfo.sin_port = port;


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
	m_portPeer = port;
	m_addrSelf = selfName.sin_addr.s_addr;
	m_portSelf = selfName.sin_port;

	m_connected = true;
	LOG("connected");

}

net::Connection::Connection(SOCKET s) {

	initEvents();

	m_sockConnected = s;
	SOCKADDR_IN self, peer;
	int namelen = sizeof(self);
	getsockname(s, reinterpret_cast<SOCKADDR *>(&self), &namelen);
	namelen = sizeof(peer);
	getpeername(s, reinterpret_cast<SOCKADDR *>(&peer), &namelen);
	m_addrPeer = peer.sin_addr.s_addr;
	m_portPeer = peer.sin_port;
	m_addrSelf = self.sin_addr.s_addr;
	m_portSelf = self.sin_port;

	m_connected = true;
}

void net::Connection::initEvents() {
	m_evt_data_avl = CreateEvent(NULL, FALSE, FALSE, nullptr);
	m_evt_queue_avl = CreateEvent(NULL, FALSE, FALSE, nullptr);
	m_evt_con_end = CreateEvent(NULL, FALSE, FALSE, nullptr);
}

net::Connection::~Connection() {
	CloseHandle(m_evt_data_avl);
	CloseHandle(m_evt_queue_avl);
	CloseHandle(m_evt_con_end);
	receiverThread.join();
}


void net::Connection::startReceiving() {
	if(m_connected && !receiverThread.joinable()) receiverThread = std::thread(&Connection::receiverWorker, this);
}

void net::Connection::pushData(const void *buffer, size_t size) {
	send(m_sockConnected, reinterpret_cast<const char *>(buffer), size, 0);
}

size_t net::Connection::pullData(void *buffer, size_t size) {
	m_mDataMod.lock();
	size_t sizePulled = m_queue.pull(size, buffer);
	SetEvent(m_evt_queue_avl);
	if(!m_queue.size()) ResetEvent(m_evt_data_avl);
	m_mDataMod.unlock();

	return sizePulled;
}

void net::Connection::receiverWorker() {
	
	LOG("receiver worker started");

	char recvBuffer[1024];
	int recvSize = 0, resumeOffs = 0;
	bool shouldStopListen = false;
	while(true) {
		if(recvSize == 0) {
			resumeOffs = 0;
			recvSize = recv(m_sockConnected, recvBuffer, 1024, 0);

			if(recvSize <= 0) {
				shouldStopListen = true;

				if(recvSize == 0) {
					LOG("connection closed by peer");
				} else {
					int err = WSAGetLastError();
					LOG("recv error %i", err);
				}
			}

			if(shouldStopListen) {
				m_connected = false; // ?
				SetEvent(m_evt_con_end); // ? {evt_listen_ended?}
				break;
			}

		} else {
			WaitForSingleObject(m_evt_queue_avl, 1000);
		}

		m_mDataMod.lock();
		SetEvent(m_evt_data_avl);
		size_t pushedSize = m_queue.push(recvSize, recvBuffer + resumeOffs);
		recvSize -= pushedSize;
		resumeOffs += pushedSize;
		if(recvSize) ResetEvent(m_evt_queue_avl);
		m_mDataMod.unlock();

	}

	LOG("receiver worker ended");
}

void net::Connection::awaitData(DWORD m) {
	HANDLE events[2] = { m_evt_data_avl, m_evt_con_end };
	WaitForMultipleObjects(2, events, false, m);
}