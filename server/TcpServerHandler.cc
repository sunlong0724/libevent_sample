/*
This exmple program provides a trivial server program that listens for TCP
connections on port 9995.  When they arrive, it writes a short message to
each client connection, and closes each connection once it is flushed.

Where possible, it exits cleanly in response to a SIGINT (ctrl-c).
*/

#include "TcpServerHandler.h"
static const int PORT = 9995;

static void conn_writecb(struct bufferevent *bev, void *user_data);
static void conn_readcb(struct bufferevent *bev, void *user_data);
static void conn_eventcb(struct bufferevent *bev, short events, void *user_data);

static void timeout_cb(evutil_socket_t fd, short event, void *arg)
{
	struct timeval newtime, difference;
	TcpServerHandler* tt = (TcpServerHandler*)arg;
	double elapsed;

	evutil_gettimeofday(&newtime, NULL);
	evutil_timersub(&newtime, &tt->m_lasttime, &difference);
	elapsed = difference.tv_sec +
		(difference.tv_usec / 1.0e6);

	//printf("timeout_cb called at %d: %.3f seconds elapsed.\n",	(int)newtime.tv_sec, elapsed);
	tt->m_lasttime = newtime;

	int32_t len = RingBuffer_available_data(tt->m_ring_buffer);

	if (len > 0) {

		if (len > MAX_PACKET_SIZE) {
			len = MAX_PACKET_SIZE;
		}

		bstring bs = RingBuffer_gets(tt->m_ring_buffer, len);

		for (auto& it : tt->m_bevs) {
			struct evbuffer *output = bufferevent_get_output(it);
			evbuffer_add(output, bs->data, bs->slen);
			//fprintf(stderr, "%04d bytes sent(%04d)!!!\n", bs->slen, len);
		}
		bdestroy(bs);
	}
	else {
		fprintf(stderr, "No data\n");
	}
}

static void listener_cb(struct evconnlistener *listener, evutil_socket_t fd,struct sockaddr *sa, int socklen, void *user_data)
{
	fprintf(stdout, "%s\n", __FUNCTION__);

	TcpServerHandler* tt = (TcpServerHandler*)user_data;
	struct event_base *base = tt->m_base;

	struct bufferevent* bev;
	bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE);
	if (!bev) {
		fprintf(stderr, "Error constructing bufferevent!");
		event_base_loopbreak(base);
		return;
	}
	bufferevent_setcb(bev, conn_readcb, NULL, conn_eventcb, user_data);
	bufferevent_enable(bev,  EV_WRITE|EV_READ);

	tt->m_bevs.insert(bev);

	/* Initalize one event */
	//if (tt->m_bevs.size() == 1) {//start a timer!!!
	//	struct timeval tv;
	//	event_assign(&tt->m_tev, base, -1, EV_PERSIST, timeout_cb, (void*)user_data);

	//	evutil_timerclear(&tv);
	//	tv.tv_sec = 0;
	//	tv.tv_usec = 1000 * MAX_SEND_TIMER;
	//	event_add(&tt->m_tev, &tv);

	//	evutil_gettimeofday(&tt->m_lasttime, NULL);
	//}
}

static void conn_writecb(struct bufferevent *bev, void *user_data)
{
	fprintf(stdout, "%s\n", __FUNCTION__);
}

static void conn_readcb(struct bufferevent *bev, void *user_data)
{
	//fprintf(stdout, "%s\n", __FUNCTION__);
	struct evbuffer *input = bufferevent_get_input(bev);

	if (evbuffer_get_length(input) == 0) {
	}

	char buf[1024] = { 0 };
	bufferevent_read(bev, buf, sizeof buf);
	fprintf(stdout, "recv:%s\n", buf);

}

static void conn_eventcb(struct bufferevent *bev, short events, void *user_data)
{
	fprintf(stdout, "%s\n", __FUNCTION__);
	if (events & BEV_EVENT_EOF) {
		printf("Connection closed.\n");
	}
	else if (events & BEV_EVENT_ERROR) {
		printf("Got an error on the connection: %s\n",	strerror(errno));/*XXX win32*/
	}
	/* None of the other events can happen here, since we haven't enabled
	* timeouts */
	TcpServerHandler* tt = (TcpServerHandler*)user_data;

	tt->m_bevs.erase(bev);
	//if (tt->m_bevs.size() == 0)
	//	event_del(&tt->m_tev);

	bufferevent_free(bev);
}

static void signal_cb(evutil_socket_t sig, short events, void *user_data)
{
	fprintf(stdout, "%s\n", __FUNCTION__);
	struct event_base *base = ((TcpServerHandler *)user_data)->m_base;

	struct timeval delay = { 1, 0 };
	printf("Caught an interrupt signal; exiting cleanly in two seconds.\n");
	event_base_loopexit(base, &delay);
}


void TcpServerHandler::start(uint16_t port) {
	m_exited = false;
	m_ring_buffer = RingBuffer_create(MAX_SEND_BUFFER_SIZE);//30Mb
	m_port = port;
	m_impl = std::thread(&TcpServerHandler::run, this);
}

void TcpServerHandler::stop() {//FIXME: stop it by ctrl+c!!!
	fprintf(stdout, "%s\n", __FUNCTION__);
	//signal_cb(SIGINT, 0, this);
	m_exited = true;
	m_impl.join();
}

int32_t TcpServerHandler::send(uint8_t* buf, size_t len) {
	for (auto& it : m_bevs) {
		struct evbuffer *output = bufferevent_get_output(it);
		bufferevent_write(it, buf, len);
		//fprintf(stderr, "%04d bytes sent(%04d)!!!\n", bs->slen, len);
	}
	return len;
}

int32_t TcpServerHandler::run() {

	struct evconnlistener	*listener;
//	struct event			*signal_event;
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

	m_base = event_base_new();
	if (!m_base) {
		fprintf(stderr, "Could not initialize libevent!\n");
		return 1;
	}
	evthread_make_base_notifiable(m_base);


	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(m_port);

	listener = evconnlistener_new_bind(m_base, listener_cb, (void *)this,
		LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, -1,
		(struct sockaddr*)&sin,
		sizeof(sin));

	if (!listener) {
		fprintf(stderr, "Could not create a listener!\n");
		return 1;
	}

	event_base_dispatch(m_base);

	evconnlistener_free(listener);

	event_base_free(m_base);

	RingBuffer_destroy(m_ring_buffer);

	printf("done\n");
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

int main(int argc, char **argv)
{
	signal(SIGINT, sig_cb);
	TcpServerHandler t;
	t.start(PORT);

	std::string str("abcdefghijklmnopqrstuvwxyz\n");
	while (g_running_flag) {
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
		t.send((uint8_t*)str.c_str(), str.size());
	}

	t.stop();

	printf("done\n");
	return 0;
}