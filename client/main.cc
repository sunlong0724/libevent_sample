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
#include <string.h>
#include <event2/util.h>
#include <stdint.h>

static const int PORT = 9995;
static const char MESSAGE[] = "Hello World From Client!!!\n";
static int g_counter = 0;

#define MAX_PACKET_SIZE			1024*1024
#define MAX_SEND_BUFFER_SIZE	MAX_PACKET_SIZE*30
#define MAX_SEND_TIMER			50 //ms

class TcpClientHandler;

#include <string>
#include <set>
#include <thread>
#include <signal.h>

extern "C" {
	#include "lcthw/RingBuffer.h"
}

static void eventcb(struct bufferevent *bev, short events, void *ptr);
static void conn_writecb(struct bufferevent *bev, void *user_data);
static void conn_readcb(struct bufferevent *bev, void *user_data);
class TcpClientHandler {
public:
	void start(const std::string& ip, const uint16_t port) {
		if (!m_exited)
			return;
		m_ip = ip;
		m_port = port;
		m_send_buffer = RingBuffer_create(MAX_SEND_BUFFER_SIZE);//30Mb

		m_impl = std::thread(&TcpClientHandler::run, this);
	}
	void stop() {
		m_exited = true;
		RingBuffer_destroy(m_send_buffer);

		m_impl.join();
	}

	int32_t send(uint8_t* buf, size_t len) {
		int32_t result = RingBuffer_write(m_send_buffer, (char*)buf, (int)len);
		if (-1 == result) {
			fprintf(stderr, "%s failed\n", __FUNCTION__);
		}
		return result;
	}

private:
	 int run() {
		struct event_base *base;
		struct bufferevent *bev;
		struct sockaddr_in sin;

#ifdef _WIN32
		WSADATA wsa_data;
		WSAStartup(0x0201, &wsa_data);
#endif

		base = event_base_new();

		memset(&sin, 0, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = htonl(0x7f000001); /* 127.0.0.1 */
		sin.sin_port = htons(m_port); /* Port 8080 */

		bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);

		bufferevent_setcb(bev, conn_readcb, conn_writecb, eventcb, this);
		bufferevent_enable(bev, EV_WRITE | EV_READ);

		if (bufferevent_socket_connect(bev,
			(struct sockaddr *)&sin, sizeof(sin)) < 0) {
			/* Error starting connection */
			bufferevent_free(bev);
			return -1;
		}

		event_base_dispatch(base);
		return 0;
	}
public:
	 struct timeval							m_lasttime;
	 RingBuffer								*m_recv_buffer;
	 RingBuffer								*m_send_buffer;
	 struct bufferevent						*m_bev;

private:


	bool									m_exited;
	std::thread								m_impl;

	std::string								m_ip;
	uint16_t								m_port;
};

bool g_running_flag = true;
void sig_cb(int sig)
{
	fprintf(stdout, "%s\n", __FUNCTION__);
	if (sig == SIGINT) {
		g_running_flag = false;
	}
}

int main(int argc, char** argv) {
	signal(SIGINT, sig_cb);
	TcpClientHandler handler;
	const std::string ip = "127.0.0.1";
	const int port = 9995;
	std::string str("ABCDEFGHIJKLMNOPQRSTUVWXYZ\n");

	handler.start(ip, port);

	while (g_running_flag) {
		handler.send((uint8_t*)str.c_str(), str.size());
		std::this_thread::sleep_for(std::chrono::milliseconds(20));

	}
	handler.stop();
	return 0;
}



static void eventcb(struct bufferevent *bev, short events, void *ptr)
{
	fprintf(stdout, "%s\n", __FUNCTION__);
	if (events & BEV_EVENT_CONNECTED) {
		/* We're connected to 127.0.0.1:8080.   Ordinarily we'd do
		something here, like start reading or writing. */
		fprintf(stdout, "1%s\n", __FUNCTION__);

		struct evbuffer *output = bufferevent_get_output(bev);
		evbuffer_add_printf(output, "%d from client!!!\n", g_counter);
	}
	else if (events & BEV_EVENT_ERROR) {
		/* An error occured while connecting. */
		fprintf(stdout, "2%s\n", __FUNCTION__);
	}
}

static void conn_writecb(struct bufferevent *bev, void *user_data)
{
	fprintf(stdout, "%s\n", __FUNCTION__);
	TcpClientHandler* tt = (TcpClientHandler*)user_data;

	struct evbuffer *output = bufferevent_get_output(bev);
	//if (evbuffer_get_length(output) == 0)
	{
		int32_t len = RingBuffer_available_data(tt->m_send_buffer);

		if (len > 0) {

			if (len > MAX_PACKET_SIZE) {
				len = MAX_PACKET_SIZE;
			}

			bstring bs = RingBuffer_gets(tt->m_send_buffer, len);
			evbuffer_add(output, bs->data, bs->slen - 1);
			fprintf(stderr, "%04d bytes sent(%04d)!!!\n", bs->slen - 1, len);
			bdestroy(bs);
		}
		else {
			fprintf(stderr, "No data\n");
		}
	}
}

static void conn_readcb(struct bufferevent *bev, void *user_data)
{
	fprintf(stdout, "%s\n", __FUNCTION__);
	struct evbuffer *input = bufferevent_get_input(bev);
	//if (evbuffer_get_length(input) == 0) {
	//	printf("flushed answer\n");
	//	bufferevent_free(bev);
	//}

	char buf[1024 * 10] = { 0 };
	bufferevent_read(bev, buf, sizeof buf);
	fprintf(stdout, "buf:%s\n", buf);
}

int main1111(int argc, char** argv)
{
	struct event_base *base;
	struct bufferevent *bev;
	struct sockaddr_in sin;

#ifdef _WIN32
	WSADATA wsa_data;
	WSAStartup(0x0201, &wsa_data);
#endif

	base = event_base_new();

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(0x7f000001); /* 127.0.0.1 */
	sin.sin_port = htons(PORT); /* Port 8080 */

	bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);

	bufferevent_setcb(bev, conn_readcb, conn_writecb, eventcb, NULL);
	bufferevent_enable(bev, EV_WRITE | EV_READ);

	if (bufferevent_socket_connect(bev,
		(struct sockaddr *)&sin, sizeof(sin)) < 0) {
		/* Error starting connection */
		bufferevent_free(bev);
		return -1;
	}

	event_base_dispatch(base);
	return 0;
}
