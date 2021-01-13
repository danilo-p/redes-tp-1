#include "common.h"

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

void usage(int argc, char **argv)
{
    printf("usage: %s <server IP> <server port>\n", argv[0]);
    printf("example: %s 127.0.0.1 51511\n", argv[0]);
    exit(EXIT_FAILURE);
}

int addrparse(const char *addrstr, const char *portstr,
              struct sockaddr_storage *storage)
{
    if (addrstr == NULL || portstr == NULL)
    {
        return -1;
    }

    uint16_t port = (uint16_t)atoi(portstr); // unsigned short
    if (port == 0)
    {
        return -1;
    }
    port = htons(port); // host to network short

    struct in_addr inaddr4; // 32-bit IP address
    if (inet_pton(AF_INET, addrstr, &inaddr4))
    {
        struct sockaddr_in *addr4 = (struct sockaddr_in *)storage;
        addr4->sin_family = AF_INET;
        addr4->sin_port = port;
        addr4->sin_addr = inaddr4;
        return 0;
    }

    struct in6_addr inaddr6; // 128-bit IPv6 address
    if (inet_pton(AF_INET6, addrstr, &inaddr6))
    {
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)storage;
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = port;
        // addr6->sin6_addr = inaddr6
        memcpy(&(addr6->sin6_addr), &inaddr6, sizeof(inaddr6));
        return 0;
    }

    return -1;
}

#define BUFSZ 1024

void *recv_thread(void *data)
{
    int socket_fd = *((int *)data);

    char buf[BUFSZ];
    printf("message: \n");
    while (1)
    {
        memset(buf, 0, BUFSZ);
        unsigned total = 0;
        size_t count;
        while (1)
        {
            count = recv(socket_fd, buf + total, BUFSZ - total, 0);
            if (count == 0)
            {
                logexit("recv");
            }
            total += count;
            if (buf[total - 1] == '\n')
            {
                buf[total - 1] = '\0';
                break;
            }
        }

        printf("\nreceived %u bytes\n", total);
        printf("%s\n\n", buf);
        printf("message: \n");
    }

    pthread_exit(EXIT_SUCCESS);
}

void *send_thread(void *data)
{
    int socket_fd = *((int *)data);

    char buf[BUFSZ];
    while (1)
    {
        memset(buf, 0, BUFSZ);
        fgets(buf, BUFSZ - 1, stdin);
        size_t count = send(socket_fd, buf, strlen(buf) + 1, 0);
        if (count != strlen(buf) + 1)
        {
            logexit("send");
        }
    }

    pthread_exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        usage(argc, argv);
    }

    struct sockaddr_storage storage;
    if (0 != addrparse(argv[1], argv[2], &storage))
    {
        usage(argc, argv);
    }

    int socket_fd = socket(storage.ss_family, SOCK_STREAM, 0);
    if (socket_fd == -1)
    {
        logexit("socket");
    }
    struct sockaddr *addr = (struct sockaddr *)(&storage);
    if (0 != connect(socket_fd, addr, sizeof(storage)))
    {
        logexit("connect");
    }

    char addrstr[BUFSZ];
    addrtostr(addr, addrstr, BUFSZ);

    printf("[log] connected to %s\n", addrstr);

    pthread_t recv_tid;
    pthread_create(&recv_tid, NULL, recv_thread, &socket_fd);

    pthread_t send_tid;
    pthread_create(&send_tid, NULL, send_thread, &socket_fd);

    pthread_join(send_tid, NULL);
    pthread_join(recv_tid, NULL);

    close(socket_fd);

    exit(EXIT_SUCCESS);
}
