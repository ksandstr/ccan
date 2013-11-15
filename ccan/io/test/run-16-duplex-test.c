/* Tests when the last connection is a duplex, and poll.c moves it over
 * deleted fd. */
#include <ccan/io/io.h>
/* Include the C files directly. */
#include <ccan/io/poll.c>
#include <ccan/io/io.c>
#include <ccan/tap/tap.h>
#include <sys/wait.h>
#include <stdio.h>

#ifndef PORT
#define PORT "65016"
#endif

struct data {
	struct io_listener *l;
	struct io_conn *writer;
	int state;
	char buf[4];
	char wbuf[32];
};

static void finish_ok(struct io_conn *conn, struct data *d)
{
	d->state++;
}

static struct io_plan write_done(struct io_conn *conn, struct data *d)
{
	d->state++;
	return io_idle();
}

static struct io_plan read_done(struct io_conn *conn, struct data *d)
{
	d->state++;
	io_close_other(d->writer);
	return io_close();
}

static void init_conn(int fd, struct data *d)
{
	struct io_conn *conn;

	ok1(d->state == 0);
	d->state++;

	memset(d->wbuf, 7, sizeof(d->wbuf));

	conn = io_new_conn(fd, io_read(d->buf, sizeof(d->buf), read_done, d));
	io_set_finish(conn, finish_ok, d);
	d->writer = io_duplex(conn, io_write(d->wbuf, sizeof(d->wbuf), write_done, d));
	ok1(d->writer);
	io_set_finish(d->writer, finish_ok, d);

	io_close_listener(d->l);
}

static int make_listen_fd(const char *port, struct addrinfo **info)
{
	int fd, on = 1;
	struct addrinfo *addrinfo, hints;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = 0;

	if (getaddrinfo(NULL, port, &hints, &addrinfo) != 0)
		return -1;

	fd = socket(addrinfo->ai_family, addrinfo->ai_socktype,
		    addrinfo->ai_protocol);
	if (fd < 0)
		return -1;

	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	if (bind(fd, addrinfo->ai_addr, addrinfo->ai_addrlen) != 0) {
		close(fd);
		return -1;
	}
	if (listen(fd, 1) != 0) {
		close(fd);
		return -1;
	}
	*info = addrinfo;
	return fd;
}

int main(void)
{
	struct data *d = malloc(sizeof(*d));
	struct addrinfo *addrinfo;
	int fd, status;

	/* This is how many tests you plan to run */
	plan_tests(10);
	d->state = 0;
	fd = make_listen_fd(PORT, &addrinfo);
	ok1(fd >= 0);
	d->l = io_new_listener(fd, init_conn, d);
	ok1(d->l);
	fflush(stdout);
	if (!fork()) {
		int i;
		char buf[32];

		io_close_listener(d->l);
		free(d);
		fd = socket(addrinfo->ai_family, addrinfo->ai_socktype,
			    addrinfo->ai_protocol);
		if (fd < 0)
			exit(1);
		if (connect(fd, addrinfo->ai_addr, addrinfo->ai_addrlen) != 0)
			exit(2);
		signal(SIGPIPE, SIG_IGN);
		for (i = 0; i < 32; i++) {
			if (read(fd, buf+i, 1) != 1)
				break;
		}
		for (i = 0; i < strlen("hellothere"); i++) {
			if (write(fd, "hellothere" + i, 1) != 1)
				break;
		}
		close(fd);
		freeaddrinfo(addrinfo);
		exit(0);
	}
	freeaddrinfo(addrinfo);
	ok1(io_loop() == NULL);
	ok1(d->state == 5);
	ok1(memcmp(d->buf, "hellothere", sizeof(d->buf)) == 0);
	free(d);

	ok1(wait(&status));
	ok1(WIFEXITED(status));
	ok1(WEXITSTATUS(status) == 0);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
