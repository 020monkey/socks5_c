#include "socket_wrap.h"
#include "socks.h"

static void ss_accept_handle(void *s, int fd, void *data, int mask)
{
	int conn_fd;
	struct conn_info conn_info;
	struct ss_conn_ctx *nc;

	conn_fd = ss_accept(fd, conn_info.ip, &conn_info.port);
	if (conn_fd < 0) {
		debug_print("ss_accetp failed!");
		return;
	}
	nc = ss_server_add_conn(s, conn_fd, AE_READABLE, &conn_info, data);
	if (nc == NULL) {
		debug_print("ss_server_add_conn failed!");
		return;
	}
}

static void echo_func(struct ss_conn_ctx *conn)
{
	int ret;
	char buf[1024], msg[128];
	size_t length;

	strcpy(msg, "<html>hello, socks5</html>");
	length = strlen(msg);
	/* Build the HTTP response */
	sprintf(buf, "HTTP/1.0 200 OK\r\n");
	sprintf(buf, "%sServer: Socks5 Server\r\n", buf);
	sprintf(buf, "%sContent-length: %d\r\n", buf, (int)length);
	sprintf(buf, "%sContent-type: text/html\r\n\r\n", buf);
	sprintf(buf, "%s%s", buf, msg);
	strcpy((char *)conn->msg->data, buf);
	debug_print("%s", conn->msg->data);
	conn->msg->used = strlen((char *)conn->msg->data);
	ret = ss_send_msg_conn(conn, 0);
	if (ret < 0)
		debug_print("ss_send_msg_conn return %d", ret);
}

static void ss_io_handle(void *conn, int fd, void *data, int mask)
{
	struct ss_conn_ctx *conn_ptr = conn;

	switch (conn_ptr->ss_conn_state) {
	case OPENING:
		ss_handshake_handle(conn_ptr);
		break;
	case CONNECTING:
		ss_msg_handle(conn_ptr, echo_func);
		break;
	case CONNECTED:
		break;
	default:
		debug_print("unknow state!");
	}
}

static void ss_remote_handle(void *s, int fd, void *data, int mask)
{
	/* TODO */
}

int main(int argc, char **argv)
{
	struct ss_server_ctx *lo_s;
	struct io_event s_event;
	struct io_event c_event;
	struct io_event r_event;
	struct conn_info remote_info = {"127.0.0.1", 8388};

	lo_s = ss_create_server(1080);
	if (lo_s == NULL)
		DIE("ss_create_server failed!");
	memset(&s_event, 0, sizeof(s_event));
	s_event.rfileproc = ss_accept_handle;
	memset(&c_event, 0, sizeof(c_event));
	c_event.rfileproc = ss_io_handle;
	s_event.para = malloc(sizeof(c_event));
	memcpy(s_event.para, &c_event, sizeof(c_event));
	memcpy(&lo_s->io_proc, &s_event, sizeof(s_event));
	memset(&r_event, 0, sizeof(r_event));
	r_event.rfileproc = ss_remote_handle;
	ss_server_add_remote(lo_s, AE_READABLE | AE_WRITABLE, &remote_info,
				&r_event);
	ss_loop(lo_s);
	ss_release_server(lo_s);
	return 0;
}
