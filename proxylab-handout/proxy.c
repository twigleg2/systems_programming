#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

void *read_write(void *sfd)
{
    int sfd2 = *((int *)sfd);
    pthread_detach(pthread_self());

    char buf[MAX_OBJECT_SIZE] = {0};
    memset(buf, 0, MAX_OBJECT_SIZE);
    ssize_t nread = 0;
    while (!strstr(buf, "\r\n\r\n"))
    {
        nread += read(sfd2, buf + nread, MAX_OBJECT_SIZE);
    }

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
    strcat(myRequest, user_agent_hdr);
    strcat(myRequest, "Connection: close");
    strcat(myRequest, "\r\n");
    strcat(myRequest, "Proxy-Connection: close");
    strcat(myRequest, "\r\n");

    strcat(myRequest, "\r\n");
    printf("myRequest:\n%s\n", myRequest);

    // Provided client code
    struct addrinfo hints;
    struct addrinfo *result;
    struct addrinfo *rp;
    int sfd3;
    /* Obtain address(es) matching host/port */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;     /* Allow IPv4 or IPv6 */
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
        sfd3 = socket(rp->ai_family, rp->ai_socktype,
                      rp->ai_protocol);
        if (sfd3 == -1)
            continue;
        if (connect(sfd3, rp->ai_addr, rp->ai_addrlen) != -1)
            break; /* Success */
        close(sfd3);
    }
    if (rp == NULL)
    { /* No address succeeded */
        fprintf(stderr, "Could not connect\n");
    }
    freeaddrinfo(result); /* No longer needed */

    //write
    int myRequestLen = strlen(myRequest);
    int bytesWritten = 0;
    while (bytesWritten != myRequestLen)
    {
        int checkErr = write(sfd3, myRequest + bytesWritten, myRequestLen - bytesWritten);
        if (checkErr == -1)
        {
            fprintf(stderr, "write error");
            exit(EXIT_FAILURE);
        }
        bytesWritten += checkErr;
    }

    char content[MAX_OBJECT_SIZE];
    int totalbytesRead = 0;
    int bytesRead = 0;
    do
    {
        bytesRead = read(sfd3, content + totalbytesRead, MAX_OBJECT_SIZE);
        totalbytesRead += bytesRead;
    } while (bytesRead != 0);

    // printf("content: %s\n", content);

    int contentLen = totalbytesRead; //TODO: can't use strlen on binary data
    bytesWritten = 0;
    while (bytesWritten != contentLen)
    {
        int checkErr = write(sfd2, content + bytesWritten, contentLen - bytesWritten);
        if (checkErr == -1)
        {
            fprintf(stderr, "write error");
            exit(EXIT_FAILURE);
        }
        bytesWritten += checkErr;
        printf("bytesWritten: %d  contentLen: %d\n", bytesWritten, contentLen);
    }
    close(sfd3);
    close(sfd2);
    free(sfd);
    return NULL;
}

// main
int main(int argc, char *argv[])
{
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
    //LOOP
    while (1)
    {
        // my server code
        struct sockaddr_storage peer_addr;
        socklen_t peer_addr_len;
        int *sfd2 = malloc(sizeof(int));
        *sfd2 = accept(sfd, (struct sockaddr *)&peer_addr, &peer_addr_len);

        pthread_t threadId;
        pthread_create(&threadId, NULL, read_write, sfd2);
    }
    //END LOOP

    // printf("%s", user_agent_hdr);
    return 0;
}
