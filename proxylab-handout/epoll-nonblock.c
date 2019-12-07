/* $begin select */
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <string.h>

#include "csapp.h"

#define MAXEVENTS 64
#define REQ_ARRAY_SIZE 512
#define MAX_OBJECT_SIZE 102400

void command(void);

enum states
{
	READ_CLIENT,
	WRITE_SERVER,
	READ_SERVER,
	WRITE_CLIENT
};

typedef struct
{
	int client_fd;							// the socket corresponding to the requesting client
	int server_fd;							// the socket corresponding to the Web server
	enum states state;						// the current state of the request (enum)
	char original_req_buf[MAX_OBJECT_SIZE]; // the buffer to store the original client request
	char modified_req_buf[MAX_OBJECT_SIZE]; // the buffer to store the modified client request
	char response_buf[MAX_OBJECT_SIZE];		// the buffer to store the server request
	int client_bytes_read;					// the total number of bytes read from the client
	int server_bytes_written;				// the number of bytes written to the server
	int server_bytes_read;					// the total number of bytes read from the server
	int client_bytes_written;				// the total number of bytes written to the client
} req_info;

req_info req_info_array[REQ_ARRAY_SIZE];
int req_info_array_size = 0; // current array size

void req_info_constructor(req_info *req)
{
	req->client_fd = -1;
	req->server_fd = -1;
	req->state = READ_CLIENT;
	req->client_bytes_read = 0;
	req->server_bytes_written = 0;
	req->server_bytes_read = 0;
	req->client_bytes_written = 0;
}

void log(FILE *file, char *url)
{
	time_t t = time(NULL);
	fprintf(file, "%ld: %s\n", t, url);
	fflush(file);
	// free(url);
}

int main(int argc, char **argv)
{
	FILE *logfile = fopen("log.txt", "a");

	int listenfd, connfd;
	socklen_t clientlen;
	struct sockaddr_storage clientaddr;
	int efd;
	struct epoll_event event;
	struct epoll_event *events;
	int i;
	int len;

	size_t n;
	char buf[MAXLINE];

	if (argc != 2)
	{
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(0);
	}

	listenfd = Open_listenfd(argv[1]);

	// set fd to non-blocking (set flags while keeping existing flags)
	if (fcntl(listenfd, F_SETFL, fcntl(listenfd, F_GETFL, 0) | O_NONBLOCK) < 0)
	{
		fprintf(stderr, "error setting socket option\n");
		exit(1);
	}

	if ((efd = epoll_create1(0)) < 0)
	{
		fprintf(stderr, "error creating epoll fd\n");
		exit(1);
	}

	event.data.fd = listenfd;
	event.events = EPOLLIN | EPOLLET; // use edge-triggered monitoring
	if (epoll_ctl(efd, EPOLL_CTL_ADD, listenfd, &event) < 0)
	{
		fprintf(stderr, "error adding event\n");
		exit(1);
	}

	/* Buffer where events are returned */
	events = calloc(MAXEVENTS, sizeof(event));

	while (1)
	{
		// wait for event to happen (no timeout)
		n = epoll_wait(efd, events, MAXEVENTS, 1000); // The spec said 1 second tieout
		// TODO:
		/*
			1. If the result was a timeout (i.e., return value from epoll_wait() is 0),
			check if a global flag has been set by a handler and, if so, break out of the loop;
			otherwise, continue.

			2. If the result was an error (i.e., return value from epoll_wait() is less than 0),
			handle the error appropriately (see the man page for epoll_wait for more).

			3. 
		*/

		for (i = 0; i < n; i++)
		{
			if ((events[i].events & EPOLLERR) ||
				(events[i].events & EPOLLHUP) ||
				(events[i].events & EPOLLRDHUP))
			{
				/* An error has occured on this fd */
				fprintf(stderr, "epoll error on fd %d\n", events[i].data.fd);
				close(events[i].data.fd);
				continue;
			}

			if (listenfd == events[i].data.fd)
			{ //line:conc:select:listenfdready
				clientlen = sizeof(struct sockaddr_storage);

				// loop and get all the connections that are available
				while ((connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen)) > 0)
				{

					// set fd to non-blocking (set flags while keeping existing flags)
					if (fcntl(connfd, F_SETFL, fcntl(connfd, F_GETFL, 0) | O_NONBLOCK) < 0)
					{
						fprintf(stderr, "error setting socket option\n");
						exit(1);
					}

					// add event to epoll file descriptor
					event.data.fd = connfd;
					event.events = EPOLLIN | EPOLLET; // use edge-triggered monitoring
					if (epoll_ctl(efd, EPOLL_CTL_ADD, connfd, &event) < 0)
					{
						fprintf(stderr, "error adding event\n");
						exit(1);
					}
					// TODO: do I make a req_info object here?
				}

				if (errno == EWOULDBLOCK || errno == EAGAIN)
				{
					// no more clients to accept()
				}
				else
				{
					perror("error accepting");
				}
			}
			else //line:conc:select:listenfdready
			{	// TODO: Julie said to re-write this part
				while ((len = recv(events[i].data.fd, buf, MAXLINE, 0)) > 0)
				{
					printf("Received %d bytes\n", len);
					send(events[i].data.fd, buf, len, 0);
				}
				if (len == 0)
				{
					// EOF received.
					// Closing the fd will automatically unregister the fd
					// from the efd
					close(events[i].data.fd);
				}
				else if (errno == EWOULDBLOCK || errno == EAGAIN)
				{
					// no more data to read()
				}
				else
				{
					perror("error reading");
				}
			}
		}
	}
	free(events);
}

/* $end select */
