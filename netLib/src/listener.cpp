#include "include.hpp"

net::TCPListener::TCPListener(serverFunc server, PORT port) {
	if(!server) {
		LOG("server dunction not set");
		return;
	}

	m_port = port;
	m_serverFunction = server;

	m_evt_sesWTC = CreateEvent(NULL, FALSE, FALSE, nullptr);

	m_accepterThread = std::thread(&TCPListener::accepterWorker, this);
	m_closerThread = std::thread(&TCPListener::closerWorker, this);
}

net::TCPListener::~TCPListener() {
	if(m_sockListener != INVALID_SOCKET) {
		closesocket(m_sockListener);
		m_sockListener = INVALID_SOCKET;
	}
	m_accepterThread.join();

	// TODO: close all sessions

	m_WTCWorkerShouldQuit = true;
	SetEvent(m_evt_sesWTC);
	m_closerThread.join();
	CloseHandle(m_evt_sesWTC);
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
			if(err != WSAEINTR) { LOG("error accept: %i", err); }
			else { LOG("ending accept loop"); }
			break;
		}

		LOG("accepted %s:%hu", IP4ToStr(acceptedCon.sin_addr.s_addr).c_str(), ntohs(acceptedCon.sin_port));

		startServer(tmp_sockAccept);

	}

	LOG("accepter thread ends");
}

void net::TCPListener::startServer(SOCKET acceptedSocket) {

	SES_IT sesIt = sesNew();
	sesIt->con = std::make_shared<Connection>(acceptedSocket);
	sesIt->worker = std::thread(&TCPListener::serverMgrWorker, this, sesIt);

}

net::TCPListener::SES_IT net::TCPListener::sesNew() {
	SES_IT it;
	m_mSessionsAll.lock();
	m_sessionsAll.emplace_back();
	it = std::prev(m_sessionsAll.end());
	LOG("started new session, active session count: %zu", m_sessionsAll.size());
	m_mSessionsAll.unlock();
	return it;
}

void net::TCPListener::serverMgrWorker(SES_IT session) {
	
	int result = m_serverFunction(session->con);

	// TODO: close connection!!!

	sesMarkClose(session);
}

void net::TCPListener::sesMarkClose(SES_IT session) {
	m_mSessionsWTC.lock();
	m_sessionsWaitingToClose.push(session);
	SetEvent(m_evt_sesWTC);
	LOG("marked session as closed");
	m_mSessionsWTC.unlock();
}

void net::TCPListener::closerWorker() {
	LOG("closer thread starts");
	bool working = true;
	while(working) {
		WaitForSingleObject(m_evt_sesWTC, INFINITE);
		m_mSessionsWTC.lock();
		size_t swtc = m_sessionsWaitingToClose.size();
		if(swtc) {
			m_sessionsWaitingToClose.pop();
			LOG("joined session");
		}
		else {
			ResetEvent(m_evt_sesWTC);
			if(m_WTCWorkerShouldQuit) working = false;
		}
		m_mSessionsWTC.unlock();
	}
	LOG("closer thread ends");
}


