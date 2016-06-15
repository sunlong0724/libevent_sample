
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#ifndef _WIN32
#include <netinet/in.h>
# ifdef _XOPEN_SOURCE_EXTENDED
#  include <arpa/inet.h>
# endif
#include <sys/socket.h>
#endif

#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>
#include <event2/event_struct.h>
#include <event2/thread.h>
#include <event2/event_struct.h>

#include <string>
#include <set>
#include <thread>
#include <queue>

extern "C" {
#include "lcthw/RingBuffer.h"
}

#define MAX_PACKET_SIZE			1024*1024
#define MAX_SEND_BUFFER_SIZE	MAX_PACKET_SIZE*30
#define MAX_SEND_TIMER			50 //ms

class TcpServerHandler {
public:
	void start(uint16_t port);
	void stop();//FIXME: stop it by ctrl+c!!!
		
	inline void join() {
		m_impl.join();
	}

	int32_t send(uint8_t* buf, size_t len);

private:
	int32_t run();

public:
	struct event_base				*m_base;
	std::set<struct bufferevent*>   m_bevs;

	RingBuffer						*m_ring_buffer;
	struct event					m_tev;
	struct timeval					m_lasttime;
	bool							m_exited;
private:

	std::thread						m_impl;
	uint16_t						m_port;
};