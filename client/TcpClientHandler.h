
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#ifndef _WIN32
#include <netinet/in.h>
# ifdef _XOPEN_SOURCE_EXTENDED
#  include <arpa/inet.h>
# endif
#include <sys/socket.h>
#endif
#include <event2/util.h>
#include <event2/thread.h>
#include <event2/event_struct.h>

#define MAX_PACKET_SIZE			1024*1024
#define MAX_SEND_BUFFER_SIZE	MAX_PACKET_SIZE*30
#define MAX_SEND_TIMER			50 //ms

class TcpClientHandler;

#include <string>
#include <set>
#include <thread>
#include <signal.h>
#include <string.h>
#include <stdint.h>

extern "C" {
#include "lcthw/RingBuffer.h"
}

class TcpClientHandler {
public:
	void start(const std::string& ip, const uint16_t port);
	void stop();

	int32_t send(uint8_t* buf, size_t len);

private:
	int run();
public:
	RingBuffer								*m_recv_buffer;
	struct bufferevent						*m_bev;
private:

	bool									m_exited;
	std::thread								m_impl;

	std::string								m_ip;
	uint16_t								m_port;
};
