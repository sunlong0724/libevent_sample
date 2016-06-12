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

static const int PORT = 9995;
static const char MESSAGE[] = "Hello World From Client!!!\n";
static int g_counter = 0;

void eventcb(struct bufferevent *bev, short events, void *ptr)
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
	//fprintf(stdout, "%s\n", __FUNCTION__);

	struct evbuffer *output = bufferevent_get_output(bev);
	if (evbuffer_get_length(output) == 0) {
		//printf("flushed answer\n");
		//bufferevent_free(bev);
		evbuffer_add_printf(output, "%d from client!!!\n", ++g_counter);
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

	char buf[1024] = { 0 };
	bufferevent_read(bev, buf, sizeof buf);
	fprintf(stdout, "buf:%s\n", buf);
}

int main(int argc, char** argv)
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