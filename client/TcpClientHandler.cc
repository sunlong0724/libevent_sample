#include "TcpClientHandler.h"

static void eventcb(struct bufferevent *bev, short events, void *ptr);
static void conn_writecb(struct bufferevent *bev, void *user_data);
static void conn_readcb(struct bufferevent *bev, void *user_data);
static void timeout_cb(evutil_socket_t fd, short event, void *arg);



void TcpClientHandler::start(const std::string& ip, const uint16_t port) {
	if (!m_exited)
		return;
	m_ip = ip;
	m_port = port;
	m_impl = std::thread(&TcpClientHandler::run, this);
}
void TcpClientHandler::stop() {
	m_exited = true;
	m_impl.join();
}

int32_t TcpClientHandler::send(uint8_t* buf, size_t len) {
	return bufferevent_write(m_bev, buf, len);
}

int32_t TcpClientHandler::run() {
	struct event_base *base;
	struct sockaddr_in sin;

#ifdef _WIN32
	WSADATA wsa_data;
	WSAStartup(0x0201, &wsa_data);
#endif

#ifdef WIN32
	evthread_use_windows_threads();//win上设置
#else
	evthread_use_pthreads();    //unix上设置
#endif

	base = event_base_new();
	evthread_make_base_notifiable(base);


	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	unsigned long ip = inet_addr(m_ip.c_str());
	sin.sin_addr.s_addr = ip;/* 127.0.0.1 */

	//sin.sin_addr.s_addr = htonl(0x7f000001);

	sin.sin_port = htons(m_port); /* Port 8080 */

	m_bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE);

	bufferevent_setcb(m_bev, conn_readcb, conn_writecb, eventcb, this);
	bufferevent_enable(m_bev, EV_WRITE | EV_READ | EV_PERSIST);

	if (bufferevent_socket_connect(m_bev,
		(struct sockaddr *)&sin, sizeof(sin)) < 0) {
		/* Error starting connection */
		bufferevent_free(m_bev);
		return -1;
	}

	event_base_dispatch(base);
	return 0;
}


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
	std::this_thread::sleep_for(std::chrono::milliseconds(2000));

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
	}
	else if (events & BEV_EVENT_ERROR) {
		/* An error occured while connecting. */
		fprintf(stdout, "2%s\n", __FUNCTION__);
	}
}


static void conn_readcb(struct bufferevent *bev, void *user_data)
{
	fprintf(stdout, "%s\n", __FUNCTION__);
	struct evbuffer *input = bufferevent_get_input(bev);

	char buf[1024 * 10] = { 0 };
	bufferevent_read(bev, buf, sizeof buf);
	fprintf(stdout, "buf:%s\n", buf);

}

static void conn_writecb(struct bufferevent *bev, void *user_data) {
	struct evbuffer *output = bufferevent_get_input(bev);
	fprintf(stdout, "%s,len:%d\n", __FUNCTION__, evbuffer_get_length(output));
}


//
//int main1111(int argc, char** argv)
//{
//	struct event_base *base;
//	struct bufferevent *bev;
//	struct sockaddr_in sin;
//
//#ifdef _WIN32
//	WSADATA wsa_data;
//	WSAStartup(0x0201, &wsa_data);
//#endif
//
//	base = event_base_new();
//
//	memset(&sin, 0, sizeof(sin));
//	sin.sin_family = AF_INET;
//	sin.sin_addr.s_addr = htonl(0x7f000001); /* 127.0.0.1 */
//	sin.sin_port = htons(PORT); /* Port 8080 */
//
//	bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
//
//	bufferevent_setcb(bev, conn_readcb, conn_writecb, eventcb, NULL);
//	bufferevent_enable(bev, EV_WRITE | EV_READ);
//
//	if (bufferevent_socket_connect(bev,
//		(struct sockaddr *)&sin, sizeof(sin)) < 0) {
//		/* Error starting connection */
//		bufferevent_free(bev);
//		return -1;
//	}
//
//	event_base_dispatch(base);
//	return 0;
//}
