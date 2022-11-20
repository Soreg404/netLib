#include "include.hpp"


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
	if(receiverThread.joinable()) receiverThread.join();
	if(m_sockConnected != INVALID_SOCKET) {
		closesocket(m_sockConnected);
		m_sockConnected = INVALID_SOCKET;
	}
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
