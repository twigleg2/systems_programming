#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

int main(int argc, char *argv[])
{
    char buf[MAX_OBJECT_SIZE];
    //server
    int sfd;
    int s;
    struct sockaddr_in ip4addr;
    struct sockaddr_storage peer_addr; //
    socklen_t peer_addr_len;           //
    ssize_t nread;                     //
    //client
    char *host;
    char *port;
    struct addrinfo hints;
    struct addrinfo *result;
    struct addrinfo *rp;

    if (argc < 2)
    {
        printf("usage: %s port\n", argv[0]);
        exit(1);
    }

    // Provided server code
    ip4addr.sin_family = AF_INET;
    ip4addr.sin_port = htons(atoi(argv[1]));
    ip4addr.sin_addr.s_addr = INADDR_ANY;
    if ((sfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket error");
        exit(EXIT_FAILURE);
    }
    if (bind(sfd, (struct sockaddr *)&ip4addr, sizeof(struct sockaddr_in)) < 0)
    {
        close(sfd);
        perror("bind error");
        exit(EXIT_FAILURE);
    }
    if (listen(sfd, 100) < 0)
    {
        close(sfd);
        perror("listen error");
        exit(EXIT_FAILURE);
    }
    // my server code
    accept(sfd, (struct sockaddr *)&peer_addr, &peer_addr_len);
    nread = recv(sfd, buf, MAX_OBJECT_SIZE, 0);
    printf("got %d bytes: %s", (int)nread, buf); //potentially won't output all the data received.

    // Provided client code
    /* Obtain address(es) matching host/port */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;     /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* TCP socket */
    hints.ai_flags = 0;
    hints.ai_protocol = 0;                        /* Any protocol */
    s = getaddrinfo(host, port, &hints, &result); //host and port come from the original http GET request
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
        sfd = socket(rp->ai_family, rp->ai_socktype,
                     rp->ai_protocol);
        if (sfd == -1)
            continue;
        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
            break; /* Success */
        close(sfd);
    }
    if (rp == NULL)
    { /* No address succeeded */
        fprintf(stderr, "Could not connect\n");
    }
    freeaddrinfo(result); /* No longer needed */

    //

    printf("%s", user_agent_hdr);
    return 0;
}
