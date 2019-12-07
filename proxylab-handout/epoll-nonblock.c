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

// You won't lose style points for including this long line in your code
static const char *user_agent_hdr =
	"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

void command(void);

FILE *logfile;
int efd;
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
} req_info_t;

typedef struct
{
};

req_info_t req_info_array[REQ_ARRAY_SIZE];
int req_info_array_size = 0; // current array size

void req_info_constructor(req_info_t *req)
{
	req->client_fd = -1;
	req->server_fd = -1;
	req->state = READ_CLIENT;
	memset(req->original_req_buf, 0, MAX_OBJECT_SIZE);
	memset(req->modified_req_buf, 0, MAX_OBJECT_SIZE);
	memset(req->response_buf, 0, MAX_OBJECT_SIZE);
	req->client_bytes_read = 0;
	req->server_bytes_written = 0;
	req->server_bytes_read = 0;
	req->client_bytes_written = 0;
}

req_info_t *find_fd(int fd)
{
	for (int i = 0; i < req_info_array_size; i++)
	{
		req_info_t *req_info = &req_info_array[i];
		if (fd == req_info->client_fd || fd == req_info->server_fd)
		{
			return req_info;
		}
	}
	return NULL;
}

// parse req_info->original_req_buf
// create req_info->modified_req_buf
// store host/port to be used later
void parse(req_info_t *req_info, char *host_url, char *host_port)
{
	char buf[MAX_OBJECT_SIZE];
	strcpy(buf, req_info->original_req_buf);

	//Begin parsing
	char *req_type = strtok(buf, " ");
	if (strcmp(req_type, "GET") != 0)
	{
		fprintf(stderr, "Bad req_type: %s\n", req_type);
		// TODO: discard request
	}
	char *http = strtok(NULL, "//");
	if (strcmp(http, "http:") != 0)
	{
		fprintf(stderr, "bad http: %s\n", http);
		// TODO: discard request
	}
	char *host = strtok(NULL, "/");
	// printf("host: %s\n", host);
	if (strlen(host) == 0)
	{
		fprintf(stderr, "bad host: %s\n", host);
		// TODO: discard request
	}
	// find if contains port
	char *port = NULL; //default
	char *colonPos = strchr(host, ':');
	if (colonPos)
	{
		port = colonPos + 1; //set port to point to one char after colon
		colonPos[0] = '\0';  //change colon to null char
	}
	// printf("host: %s, port: %s\n", host, port);
	char *path = strtok(NULL, "\r\n"); //path contains the rest of the first line, including HTTP/1.1
	char *pathEnd = strchr(path, ' '); //find the first space, which is the real end of the path.
	pathEnd[0] = '\0';
	if (path == NULL)
	{
		fprintf(stderr, "bad request, not HTTP/1.1");
		// TODO discard request;
	}
	// printf("path: %s\n\n", path);
	// TODO: verify that HTTP/1.1 was sent, else invalid request

	/* begin putting together my request*/
	char myRequest[MAX_OBJECT_SIZE] = "";
	strcat(myRequest, req_type);
	strcat(myRequest, " /"); //path is missing the beggining slash, so adding it here.
	strcat(myRequest, path);
	strcat(myRequest, " ");
	strcat(myRequest, "HTTP/1.0\r\n");

	int hasHostHeader = 0;
	char *header;
	while ((header = strtok(NULL, "\r\n")) != NULL)
	{
		char *colon = strchr(header, ':');
		colon[0] = '\0'; // temporary

		if (strcmp(header, "Connection") == 0)
		{
			//throw away, I'll add my own later
			continue;
		}
		if (strcmp(header, "Proxy-Connection") == 0)
		{
			//throw away, I'll add my own later
			continue;
		}
		if (strcmp(header, "User-Agent") == 0)
		{
			//throw away, I'll add my own later
			continue;
		}
		if (strcmp(header, "Host") == 0)
		{
			hasHostHeader = 1;
		}

		colon[0] = ':';
		strcat(myRequest, header);
		strcat(myRequest, "\r\n");
	}
	if (!hasHostHeader)
	{
		strcat(myRequest, host);
		strcat(myRequest, "\r\n");
	}
	// headers required by the lab
	strcat(myRequest, user_agent_hdr);
	strcat(myRequest, "Connection: close");
	strcat(myRequest, "\r\n");
	strcat(myRequest, "Proxy-Connection: close");
	strcat(myRequest, "\r\n");
	strcat(myRequest, "\r\n");
	// End parsing

	host_url = host;
	host_port = port;
	strcpy(req_info->modified_req_buf, myRequest);
}

int connect_to_server(char *host, char *port)
{
	// Provided client code
	struct addrinfo hints;
	struct addrinfo *result;
	struct addrinfo *rp;
	int hostfd;
	/* Obtain address(es) matching host/port */
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;	 /* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM; /* TCP socket */
	hints.ai_flags = 0;
	hints.ai_protocol = 0; /* Any protocol */
	//host and port come from the original http GET request
	int s = getaddrinfo(host, port ? port : "80", &hints, &result);
	if (s != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(EXIT_FAILURE);
	}
	/* getaddrinfo() returns a list of address structures.
    Try each address until we successfully connect(2).
    If socket(2) (or connect(2)) fails, we (close the socket
    and) try the next address. */
	for (rp = result; rp != NULL; rp = rp->ai_next)
	{
		hostfd = socket(rp->ai_family, rp->ai_socktype,
						rp->ai_protocol);
		if (hostfd == -1)
			continue;
		if (connect(hostfd, rp->ai_addr, rp->ai_addrlen) != -1)
			break; /* Success */
		close(hostfd);
	}
	if (rp == NULL)
	{ /* No address succeeded */
		fprintf(stderr, "Could not connect\n");
	}
	freeaddrinfo(result); /* No longer needed */

	return hostfd;
}

void read_client(req_info_t *req_info)
{
	// char buf[MAX_OBJECT_SIZE];
	// ssize_t nread = 0;
	while (!strstr(req_info->original_req_buf, "\r\n\r\n"))
	{
		int bytes_read = read(req_info->client_fd, req_info->original_req_buf + req_info->client_bytes_read, MAX_OBJECT_SIZE);
		if (bytes_read == -1)
		{
			if (errno == EWOULDBLOCK || errno == EAGAIN)
			{
				// can't read more data
				return;
			}
			else
			{
				perror("error reading");
				exit(errno);
			}
		}
		// else if (bytes_read == 0)
		// {
		//		//treat zero as a positive for now;
		// }
		else
		{
			req_info->client_bytes_read += bytes_read;
		}
	}
	// while loop exits naturally

	char host_url[MAX_OBJECT_SIZE];
	char host_port[MAX_OBJECT_SIZE];
	memset(host_url, 0, MAX_OBJECT_SIZE);
	memset(host_port, 0, MAX_OBJECT_SIZE);

	parse(req_info, host_url, host_port);
	log(req_info->original_req_buf);
	req_info->server_fd = connect_to_server(host_url, host_port);

	struct epoll_event event;
	event.data.fd = req_info->server_fd;
	event.events = EPOLLOUT | EPOLLET; // use edge-triggered monitoring
	if (epoll_ctl(efd, EPOLL_CTL_ADD, req_info->server_fd, &event) < 0)
	{
		fprintf(stderr, "error adding event\n");
		exit(1);
	}
	req_info->state = WRITE_SERVER;
	return;
}

void write_server(req_info_t *req_info)
{
	//write request
	int modified_req_len = strlen(req_info->modified_req_buf);
	while (req_info->server_bytes_written != modified_req_len)
	{
		int bytes_written = write(req_info->server_fd, req_info->modified_req_buf + req_info->server_bytes_written, modified_req_len - req_info->server_bytes_written);
		if (bytes_written == -1)
		{
			if (errno == EWOULDBLOCK || errno == EAGAIN)
			{
				// can't write more data
				return;
			}
			else
			{
				perror("error writting");
				exit(errno);
			}
		}
		// else if (bytes_written == 0)
		// {
		//		//treat zero as a positive for now;
		// }
		else
		{
			req_info->server_bytes_written += bytes_written;
		}
	}
	// while loop ends naturally

	struct epoll_event event;
	event.data.fd = req_info->server_fd;
	event.events = EPOLLIN | EPOLLET; // use edge-triggered monitoring
	if (epoll_ctl(efd, EPOLL_CTL_MOD, req_info->server_fd, &event) < 0)
	{
		fprintf(stderr, "error adding event\n");
		exit(1);
	}
	req_info->state = READ_SERVER;
	return;
}

void read_server(req_info_t *req_info)
{
	//read response
	int bytes_read = 0;
	do
	{
		bytes_read = read(req_info->server_fd, req_info->response_buf + req_info->server_bytes_read, MAX_OBJECT_SIZE);
		if (bytes_read == -1)
		{
			if (errno == EWOULDBLOCK || errno == EAGAIN)
			{
				// can't read more data
				return;
			}
			else
			{
				perror("error reading");
				exit(errno);
			}
		}
		else
		{
			req_info->server_bytes_read += bytes_read;
		}
	} while (bytes_read != 0);
	//loop ends naturally

	struct epoll_event event;
	event.data.fd = req_info->client_fd;
	event.events = EPOLLOUT | EPOLLET; // use edge-triggered monitoring
	if (epoll_ctl(efd, EPOLL_CTL_MOD, req_info->client_fd, &event) < 0)
	{
		fprintf(stderr, "error adding event\n");
		exit(1);
	}
	req_info->state = WRITE_CLIENT;
	close(req_info->server_fd); // close file descriptor
	req_info->server_fd = -1;   // set fd to -1 so it won't be found in search

	return;
}

void write_client(req_info_t *req_info)
{
	//write response
	int contentLen = req_info->server_bytes_read;
	while (req_info->client_bytes_written != contentLen)
	{
		int bytesWritten = write(req_info->client_fd, req_info->response_buf + req_info->client_bytes_written, contentLen - req_info->client_bytes_written);
		if (bytesWritten == -1)
		{
			if (errno == EWOULDBLOCK || errno == EAGAIN)
			{
				// can't write more data
				return;
			}
			else
			{
				perror("error writting");
				exit(errno);
			}
		}
		// else if (bytes_written == 0)
		// {
		//		//treat zero as a positive for now;
		// }
		else
		{
			req_info->client_bytes_written += bytesWritten;
		}
	}
	//loop ends naturally

	req_info->state = -1;		//done
	close(req_info->client_fd); // close file descriptor
	req_info->client_fd = -1;   // set fd to -1 so it won't be found in search

	return;
}

void log(char *buf)
{
	char buf_copy[MAX_OBJECT_SIZE];
	strcpy(buf_copy, buf);
	strtok(buf_copy, " "); // throw away REST type, ex. GET
	char *url = strtok(NULL, " ");
	time_t t = time(NULL);
	fprintf(logfile, "%ld: %s\n", t, url);
	// fflush(logfile);
}

int main(int argc, char **argv)
{
	logfile = fopen("log.txt", "a");

	int listenfd, connfd;
	socklen_t clientlen;
	struct sockaddr_storage clientaddr;
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

			if (events[i].data.fd == listenfd)
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
					req_info_t req_info;
					req_info_constructor(&req_info);
					req_info.client_fd = connfd;
					req_info_array[req_info_array_size] = req_info;
					req_info_array_size++;
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
			{
				// given a file decriptor, events[i].data.fd
				// search for it in the req_info_array
				req_info_t *req_info = find_fd(events[i].data.fd);
				if (req_info == NULL)
				{
					fprintf(stderr, "NULL pointer exception!\n");
					exit(1);
				}
				// check its state with a switch statement
				switch (req_info->state)
				{
				case READ_CLIENT:
					read_client(req_info);
					break;
				case WRITE_SERVER:
					write_server(req_info);
					break;
				case READ_SERVER:
					read_server(req_info);
					break;
				case WRITE_CLIENT:
					write_client(req_info);
					break;
				default:
					fprintf(stderr, "The state doesn't match! state: %d\n", req_info->state);
				}

				// while ((len = recv(events[i].data.fd, buf, MAXLINE, 0)) > 0)
				// {
				// 	printf("Received %d bytes\n", len);
				// 	send(events[i].data.fd, buf, len, 0);
				// }
				// if (len == 0)
				// {
				// 	// EOF received.
				// 	// Closing the fd will automatically unregister the fd
				// 	// from the efd
				// 	close(events[i].data.fd);
				// }
				// else if (errno == EWOULDBLOCK || errno == EAGAIN)
				// {
				// 	// no more data to read()
				// }
				// else
				// {
				// 	perror("error reading");
				// }
			}
		}
	}
	free(events);
}

/* $end select */
