#include "common.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>

#define BUFSZ 1024

void usage(int argc, char **argv)
{
    printf("usage: %s <server port>\n", argv[0]);
    printf("example: %s v4 51511\n", argv[0]);
    exit(EXIT_FAILURE);
}

int sockaddr_init(const char *portstr,
                         struct sockaddr_storage *storage)
{
    uint16_t port = (uint16_t)atoi(portstr); // unsigned short
    if (port == 0)
    {
        return -1;
    }
    port = htons(port); // host to network short

    memset(storage, 0, sizeof(*storage));
    struct sockaddr_in *addr4 = (struct sockaddr_in *)storage;
    addr4->sin_family = AF_INET;
    addr4->sin_addr.s_addr = INADDR_ANY;
    addr4->sin_port = port;
    return 0;
}

struct client_data
{
    int client_socket_fd;
    struct sockaddr_storage storage;
};

void *client_thread(void *data)
{
    struct client_data *cdata = (struct client_data *)data;
    struct sockaddr *client_addr = (struct sockaddr *)(&cdata->storage);

    char caddrstr[BUFSZ];
    addrtostr(client_addr, caddrstr, BUFSZ);
    printf("[log] connection from %s\n", caddrstr);

    char buf[BUFSZ];
    memset(buf, 0, BUFSZ);
    size_t count = recv(cdata->client_socket_fd, buf, BUFSZ - 1, 0);
    printf("[msg] %s, %d bytes: %s\n", caddrstr, (int)count, buf);

    sprintf(buf, "remote endpoint: %.1000s\n", caddrstr);
    count = send(cdata->client_socket_fd, buf, strlen(buf) + 1, 0);
    if (count != strlen(buf) + 1)
    {
        logexit("send");
    }
    close(cdata->client_socket_fd);

    pthread_exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        usage(argc, argv);
    }

    char *port = argv[1];

    struct sockaddr_storage storage;
    if (0 != sockaddr_init(port, &storage))
    {
        usage(argc, argv);
    }

    int socket_fd;
    socket_fd = socket(storage.ss_family, SOCK_STREAM, 0);
    if (socket_fd == -1)
    {
        logexit("socket");
    }

    int enable = 1;
    if (0 != setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)))
    {
        logexit("setsockopt");
    }

    struct sockaddr *addr = (struct sockaddr *)(&storage);
    if (0 != bind(socket_fd, addr, sizeof(storage)))
    {
        logexit("bind");
    }

    if (0 != listen(socket_fd, 10))
    {
        logexit("listen");
    }

    char addrstr[BUFSZ];
    addrtostr(addr, addrstr, BUFSZ);
    printf("bound to %s, waiting connections\n", addrstr);

    while (1)
    {
        struct sockaddr_storage client_storage;
        struct sockaddr *client_addr = (struct sockaddr *)(&client_storage);
        socklen_t client_addrlen = sizeof(client_storage);

        int client_socket_fd = accept(socket_fd, client_addr, &client_addrlen);
        if (client_socket_fd == -1)
        {
            logexit("accept");
        }

        struct client_data *cdata = malloc(sizeof(*cdata));
        if (!cdata)
        {
            logexit("malloc");
        }
        cdata->client_socket_fd = client_socket_fd;
        memcpy(&(cdata->storage), &client_storage, sizeof(client_storage));

        pthread_t tid;
        pthread_create(&tid, NULL, client_thread, cdata);
    }

    close(socket_fd);

    exit(EXIT_SUCCESS);
}
