#include "include.hpp"

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
