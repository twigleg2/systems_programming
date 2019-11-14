#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <regex.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

int main(int argc, char *argv[])
{
    char buf[MAX_OBJECT_SIZE];

    if (argc < 2)
    {
        printf("usage: %s port\n", argv[0]);
        exit(1);
    }

    // Provided server code
    struct sockaddr_in ip4addr;
    ip4addr.sin_family = AF_INET;
    ip4addr.sin_port = htons(atoi(argv[1]));
    ip4addr.sin_addr.s_addr = INADDR_ANY;
    int sfd;
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
    struct sockaddr_storage peer_addr;
    socklen_t peer_addr_len;
    ssize_t nread;
    int sfd2 = accept(sfd, (struct sockaddr *)&peer_addr, &peer_addr_len);
    nread = read(sfd2, buf, MAX_OBJECT_SIZE);       // TODO: read in a loop?
    printf("got %d bytes:\n%s\n", (int)nread, buf); // TODO: potentially won't output all the data received.

    //begin parsing
    char *req_type = strtok(buf, " ");
    if (strcmp(req_type, "GET") != 0)
    {
        printf("Bad req_type: %s\n", req_type);
        // TODO: discard request
    }
    char *http = strtok(NULL, "//");
    if (strcmp(http, "http:") != 0)
    {
        printf("bad http: %s\n", http);
        // TODO: discard request
    }
    char *host = strtok(NULL, "/");
    if (strlen(host) == 0)
    {
        printf("bad host: %s\n", host);
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
    printf("host: %s, port: %s\n", host, port);
    char *path = strtok(NULL, "\r\n"); //path contains the rest of the first line, including HTTP/1.1
    char *pathEnd = strchr(path, ' '); //find the first space, which is the real end of the path.
    pathEnd[0] = '\0';
    if (path == NULL)
    {
        printf("bad request, not HTTP/1.1");
        // TODO discard request;
    }
    printf("path: %s\n\n", path);
    // TODO: verify that HTTP/1.1 was sent, else invalid request

    // begin putting together my request
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
    strcat(myRequest, "User-Agent: ");
    strcat(myRequest, user_agent_hdr);
    strcat(myRequest, "Connection: close");
    strcat(myRequest, "\r\n");
    strcat(myRequest, "Proxy-Connection: close");
    strcat(myRequest, "\r\n");

    strcat(myRequest, "\r\n");
    printf("myRequest:\n%s\n", myRequest);

    // // Provided client code
    // char *host;
    // char *port;
    // struct addrinfo hints;
    // struct addrinfo *result;
    // struct addrinfo *rp;
    // /* Obtain address(es) matching host/port */
    // memset(&hints, 0, sizeof(struct addrinfo));
    // hints.ai_family = AF_UNSPEC;     /* Allow IPv4 or IPv6 */
    // hints.ai_socktype = SOCK_STREAM; /* TCP socket */
    // hints.ai_flags = 0;
    // hints.ai_protocol = 0;                            /* Any protocol */
    // int s = getaddrinfo(host, port, &hints, &result); //host and port come from the original http GET request
    // if (s != 0)
    // {
    //     fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
    //     exit(EXIT_FAILURE);
    // }
    // /* getaddrinfo() returns a list of address structures.
    // Try each address until we successfully connect(2).
    // If socket(2) (or connect(2)) fails, we (close the socket
    // and) try the next address. */
    // for (rp = result; rp != NULL; rp = rp->ai_next)
    // {
    //     sfd = socket(rp->ai_family, rp->ai_socktype,
    //                  rp->ai_protocol);
    //     if (sfd == -1)
    //         continue;
    //     if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
    //         break; /* Success */
    //     close(sfd);
    // }
    // if (rp == NULL)
    // { /* No address succeeded */
    //     fprintf(stderr, "Could not connect\n");
    // }
    // freeaddrinfo(result); /* No longer needed */

    // //

    printf("%s", user_agent_hdr);
    return 0;
}
