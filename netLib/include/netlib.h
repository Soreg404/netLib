#pragma once

#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <memory>
#include <string>
#include <list>
#include <queue>

#include <WinSock2.h>
#include <Ws2tcpip.h>

#include <windows.h>



#ifdef _DEBUG
#include <iostream>
extern std::mutex LOG_MUTEX;
static size_t APP_START_TIME = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
#define LOG(f, ...) {\
	size_t t = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() - APP_START_TIME;\
	auto id = std::this_thread::get_id();\
	LOG_MUTEX.lock();\
	std::cout << "$ [" << t << "] (" << id << "): ";\
	printf(f "\n", ## __VA_ARGS__);\
	LOG_MUTEX.unlock();\
}
#else
#define LOG(...) {}
#endif


namespace net {

	struct NetLib {
		NetLib();
		~NetLib();
	};

	class Carousel {
	public:
		Carousel(size_t capacity);

		// TODO:
		void resize(size_t capacity);

		inline size_t size() const { return m_size; }

		~Carousel();
		size_t push(size_t size, const void *buffer);
		size_t pull(size_t size, void *buffer = nullptr);
		char &at(size_t index);
	private:
		size_t iTransl(size_t index) const;
		size_t m_capacity = 0, m_size = 0, m_head = 0;
		std::vector<char> m_buffer;
	};

	typedef unsigned short PORT;
	typedef unsigned int IP4;
	//typedef unsigned long long IP6;

	byte IP4Octet(IP4 addr, unsigned int octet);
	IP4 strToIP4(const char *str);
	std::string IP4ToStr(IP4 addr);

	struct Connection {
		friend class TCPListener;

		Connection(IP4 addr, PORT port);
		Connection(SOCKET s);
		~Connection();

		void startReceiving();

		void pushData(const void *buffer, size_t size);
		size_t pullData(void *buffer, size_t size);

		void awaitData(DWORD waitTime);

		inline bool isConnected() const { return m_connected; }
		inline operator bool() const { return m_connected; }

	private:
		SOCKET m_sockConnected = 0;
		bool m_connected = false;
		IP4 m_addrSelf = 0, m_addrPeer = 0;
		PORT m_portSelf = 0, m_portPeer = 0;

		std::thread receiverThread;
		void receiverWorker();

		std::mutex m_mDataMod;
		Carousel m_queue{1024};

		void initEvents();
		HANDLE m_evt_data_avl, m_evt_queue_avl, m_evt_con_end;
	};

	typedef std::shared_ptr<Connection> CON_PTR;

	typedef int (serverFunc)(CON_PTR);

	class TCPListener {

	public:

		TCPListener(serverFunc server, PORT port);
		~TCPListener();

	private:

		SOCKET m_sockListener = 0;

		PORT m_port = 0;
		serverFunc *m_serverFunction = nullptr;

		std::thread m_accepterThread;
		void accepterWorker();

		void startServer(SOCKET acceptedSocket);

		struct Session {
			Session() = default;
			CON_PTR con;
			std::thread worker;
			inline ~Session() { if(worker.joinable()) worker.join(); }
		};
		std::mutex m_mSessionsAll;
		std::list<Session> m_sessionsAll;
		typedef std::list<Session>::iterator SES_IT;
		SES_IT sesNew();

		void serverMgrWorker(SES_IT session);

		std::mutex m_mSessionsWTC; // how unfortunate name
		std::queue<SES_IT> m_sessionsWaitingToClose;
		void sesMarkClose(SES_IT session);

		HANDLE m_evt_sesWTC;
		std::atomic<bool> m_WTCWorkerShouldQuit = false;
		std::thread m_closerThread;
		void closerWorker();

	};

}