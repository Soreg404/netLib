#pragma once

#include <vector>
#include <thread>
#include <mutex>
#include <chrono>

#include <windows.h>

#ifdef _DEBUG
#include <iostream>
static std::mutex LOG_MUTEX;
static size_t APP_START_TIME = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
#define LOG(f, ...) {\
	size_t t = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() - APP_START_TIME;\
	auto id = std::this_thread::get_id(); \
	LOG_MUTEX.lock();\
	std::cout << "$ [" << t << "] (" << id << "): ";\
	printf(f "\n", ## __VA_ARGS__);\
	LOG_MUTEX.unlock();\
}
#else
#define LOG(...) ;
#endif


namespace net {

	void init();

	struct Connection {

	};

	typedef void (*serverFunc)(Connection);

	class TCPListener {

	public:

		TCPListener();
		~TCPListener();

		int start(serverFunc server, USHORT port);

	private:

		SOCKET m_sockAccept = 0;

		USHORT m_port = 0;
		serverFunc m_serverFunction = nullptr;

		std::thread m_accepterThread;

		void accepterWorker();

	};

}