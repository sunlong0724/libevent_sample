/*
This exmple program provides a trivial server program that listens for TCP
connections on port 9995.  When they arrive, it writes a short message to
each client connection, and closes each connection once it is flushed.

Where possible, it exits cleanly in response to a SIGINT (ctrl-c).
*/


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

#include <string>

static const char MESSAGE[] = "Hello World From Server!!!\n";

static const int PORT = 9995;

static void listener_cb(struct evconnlistener *, evutil_socket_t,
	struct sockaddr *, int socklen, void *);
static void conn_writecb(struct bufferevent *, void *);
static void conn_readcb(struct bufferevent *bev, void *user_data);
static void conn_eventcb(struct bufferevent *, short, void *);
static void signal_cb(evutil_socket_t, short, void *);





#include <thread>
#include <queue>

class TransportThread {
public:
	bool init(const char* params) {//FIXME: port and addr!!!
	}
	void start(/*_SourceDataCallback ReadNextFrameCallback, _SinkDataCallback WriteFrameCallback*/) {
		m_send_flag = false;
		m_impl = std::thread(&TransportThread::run, this);
	}
	void stop() {
		signal_cb(SIGINT, 0, this->m_base);
		m_impl.join();
	}
	void join() {
		m_impl.join();
	}

	int32_t send(uint8_t* dest, size_t len) {
		m_send_buf.emplace_back(std::vector<uint8_t>(dest, dest+len) );
	
		return (int32_t)len;
	}

private:
	int32_t run() {

		struct evconnlistener	*listener;
		struct event			*signal_event;
		struct sockaddr_in sin;
#ifdef _WIN32
		WSADATA wsa_data;
		WSAStartup(0x0201, &wsa_data);
#endif

		m_base = event_base_new();
		if (!m_base) {
			fprintf(stderr, "Could not initialize libevent!\n");
			return 1;
		}

		memset(&sin, 0, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_port = htons(PORT);

		listener = evconnlistener_new_bind(m_base, listener_cb, (void *)this,
			LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, -1,
			(struct sockaddr*)&sin,
			sizeof(sin));

		if (!listener) {
			fprintf(stderr, "Could not create a listener!\n");
			return 1;
		}

		signal_event = evsignal_new(m_base, SIGINT, signal_cb, (void *)this);

		if (!signal_event || event_add(signal_event, NULL) < 0) {
			fprintf(stderr, "Could not create/add a signal event!\n");
			return 1;
		}

		event_base_dispatch(m_base);

		evconnlistener_free(listener);
		event_free(signal_event);
		event_base_free(m_base);

		printf("done\n");
		return 0;
	}

public:
	struct event_base		*m_base;
	struct bufferevent		*m_bev;
	std::deque<std::vector<uint8_t> > m_send_buf;
	struct event			m_tev;

private:
	//bool					m_exited;
	std::thread				m_impl;
	bool					m_send_flag;
};
struct timeval lasttime;

static void timeout_cb(evutil_socket_t fd, short event, void *arg)
{
	struct timeval newtime, difference;
	TransportThread* tt = (TransportThread*)arg;
	double elapsed;

	evutil_gettimeofday(&newtime, NULL);
	evutil_timersub(&newtime, &lasttime, &difference);
	elapsed = difference.tv_sec +
		(difference.tv_usec / 1.0e6);

	printf("timeout_cb called at %d: %.3f seconds elapsed.\n",
		(int)newtime.tv_sec, elapsed);
	lasttime = newtime;

	//if (!event_is_persistent) {
	//	struct timeval tv;
	//	evutil_timerclear(&tv);
	//	tv.tv_sec = 2;
	//	event_add(timeout, &tv);
	//}

	struct evbuffer *output = bufferevent_get_output(tt->m_bev);
	if (tt->m_send_buf.size() > 0) {
		tt->m_send_buf.front();
		evbuffer_add(output, tt->m_send_buf.front().data(), tt->m_send_buf.front().size());
		tt->m_send_buf.pop_front();
		fprintf(stderr, "send a frame!\n");
	}
	else {
		fprintf(stderr, "No data\n");
	}
}

static void listener_cb(struct evconnlistener *listener, evutil_socket_t fd,struct sockaddr *sa, int socklen, void *user_data)
{
	fprintf(stdout, "%s\n", __FUNCTION__);

	TransportThread* tt = (TransportThread*)user_data;
	struct event_base *base = tt->m_base;
	struct timeval tv;

	tt->m_bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
	if (!tt->m_bev) {
		fprintf(stderr, "Error constructing bufferevent!");
		event_base_loopbreak(base);
		return;
	}
	bufferevent_setcb(tt->m_bev, conn_readcb, conn_writecb, conn_eventcb, user_data);
	bufferevent_enable(tt->m_bev, EV_WRITE | EV_READ);

	/* Initalize one event */
	event_assign(&tt->m_tev, base, -1, EV_PERSIST, timeout_cb, (void*)user_data);

	evutil_timerclear(&tv);
	tv.tv_sec = 2;
	event_add(&tt->m_tev, &tv);

	evutil_gettimeofday(&lasttime, NULL);
}

static void conn_writecb(struct bufferevent *bev, void *user_data)
{
	fprintf(stdout, "%s\n", __FUNCTION__);
	struct evbuffer *output = bufferevent_get_output(bev);
	//if (evbuffer_get_length(output) == 0) 
	{
		//printf("flushed answer\n");
		//bufferevent_free(bev);
		TransportThread* tt = (TransportThread*)user_data;
		if (tt->m_send_buf.size() > 0) {
			tt->m_send_buf.front();
			evbuffer_add(output, tt->m_send_buf.front().data(), tt->m_send_buf.front().size());
			tt->m_send_buf.pop_front();
			fprintf(stderr, "send a frame!\n");
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

	if (evbuffer_get_length(input) == 0) {
		//printf("flushed answer\n");
		//bufferevent_free(bev);
	}

	char buf[1024] = { 0 };
	bufferevent_read(bev, buf, sizeof buf);
	fprintf(stdout, "buf:%s\n", buf);

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
	bufferevent_free(bev);
}

static void signal_cb(evutil_socket_t sig, short events, void *user_data)
{
	fprintf(stdout, "%s\n", __FUNCTION__);
	struct event_base *base = ((TransportThread *)user_data)->m_base;

	struct timeval delay = { 2, 0 };
	printf("Caught an interrupt signal; exiting cleanly in two seconds.\n");
	event_base_loopexit(base, &delay);
}


int main(int argc, char **argv)
{

	TransportThread t;
	t.start();

	std::string str("abcdefghijklmnopqrstuvwxyz");
	while (true) {
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
		t.send((uint8_t*)str.c_str(), str.size());
	}

	t.join();

	printf("done\n");
	return 0;
}