#include "common.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vector>
#include <algorithm>
#include <iostream>
#include <regex>

#include <sys/socket.h>
#include <sys/types.h>

#define BUFSZ 501

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
    struct server_data *sdata;
    pthread_t tid;
    std::vector<std::string> tags;
};

struct server_data
{
    int socket_fd;
    std::vector<struct client_data *> clients_data;
};

void send_message(struct client_data *cdata, char buf[BUFSZ])
{
    // adding 1 to strlen(buf) would send \0
    size_t count = send(cdata->client_socket_fd, buf, strlen(buf), 0);
    if (count != strlen(buf))
    {
        struct sockaddr *client_addr = (struct sockaddr *)(&cdata->storage);
        char caddrstr[BUFSZ];
        addrtostr(client_addr, caddrstr, BUFSZ);
        printf("[log] error sending message to %s\n", caddrstr);
    }
}

std::vector<std::string> message_tags(char buf[BUFSZ])
{
    std::vector<std::string> tags;

    std::string buf_str(buf);

    std::istringstream iss(buf_str);
    std::vector<std::string> results((std::istream_iterator<std::string>(iss)),
                                     std::istream_iterator<std::string>());

    std::regex rgx("^#([a-zA-z]+)$");
    for (int i = 0; i < (int)results.size(); i++)
    {
        std::smatch m;
        if (std::regex_search(results.at(i), m, rgx))
        {
            tags.push_back(m.str(1));
        }
    }

    return tags;
}

void broadcast_message(struct client_data *client_sender_data, char buf[BUFSZ])
{
    for (int i = 0; i < (int)client_sender_data->sdata->clients_data.size(); i++)
    {
        struct client_data *cdata = client_sender_data->sdata->clients_data.at(i);
        if (cdata == client_sender_data)
            continue;

        std::vector<std::string> buf_tags = message_tags(buf);
        for (int i = 0; i < (int)cdata->tags.size(); i++)
        {
            if (std::find(buf_tags.begin(), buf_tags.end(), cdata->tags.at(i)) != buf_tags.end())
            {
                send_message(cdata, buf);
                break;
            }
        }
    }
}

void kill_server(struct server_data *sdata)
{
    for (int i = 0; i < (int)sdata->clients_data.size(); i++)
    {
        struct client_data *cdata = sdata->clients_data.at(i);
        close(cdata->client_socket_fd);
        free(cdata);
    }

    sdata->clients_data.clear();
    sdata->clients_data.shrink_to_fit();

    close(sdata->socket_fd);
    printf("killed\n");
    exit(EXIT_SUCCESS);
}

bool validate_message_content(char buf[BUFSZ], int size)
{
    std::regex rgx("[^A-Za-z0-9,.?!:;+\\-*/=@#$%()[\\]{} \n]");
    return !(buf[size - 1] != '\n' || std::regex_search(buf, rgx));
}

bool process_subscription_message(struct client_data *cdata, char buf[BUFSZ], int size)
{
    buf[size - 1] = '\0'; // eliminate line break
    std::regex rgx("^[+\\-][A-Za-z]+$");
    if (!std::regex_search(buf, rgx))
        return false;

    char confirmation[BUFSZ];
    switch (buf[0])
    {
    case '+':
        if ((std::find(cdata->tags.begin(), cdata->tags.end(), buf + 1) == cdata->tags.end()))
        {
            cdata->tags.push_back(buf + 1);
            sprintf(confirmation, "subscribed %s\n", buf);
            send_message(cdata, confirmation);
        }
        else
        {
            sprintf(confirmation, "already subscribed %s\n", buf);
            send_message(cdata, confirmation);
        }
        break;
    case '-':
        if ((std::find(cdata->tags.begin(), cdata->tags.end(), buf + 1) != cdata->tags.end()))
        {
            cdata->tags.erase(std::remove(cdata->tags.begin(), cdata->tags.end(), buf + 1), cdata->tags.end());
            sprintf(confirmation, "unsubscribed %s\n", buf);
            send_message(cdata, confirmation);
        }
        else
        {
            sprintf(confirmation, "not subscribed %s\n", buf);
            send_message(cdata, confirmation);
        }
        break;
    default:
        return false;
    }

    return true;
}

bool process_message(struct client_data *client_sender_data, char buf[BUFSZ], int size)
{
    if (!validate_message_content(buf, size))
        return false;

    if (strcmp(buf, "##kill\n") == 0)
    {
        kill_server(client_sender_data->sdata);
        return false;
    }

    if (buf[0] == '+' || buf[0] == '-')
        return process_subscription_message(client_sender_data, buf, size);

    broadcast_message(client_sender_data, buf);
    return true;
}

void *client_thread(void *data)
{
    struct client_data *cdata = (struct client_data *)data;
    struct sockaddr *client_addr = (struct sockaddr *)(&cdata->storage);

    char caddrstr[BUFSZ];
    addrtostr(client_addr, caddrstr, BUFSZ);
    printf("[log] connection from %s\n", caddrstr);

    while (1)
    {
        char buf[BUFSZ];
        memset(buf, 0, BUFSZ);

        printf("[msg] waiting for client %s\n", caddrstr);
        size_t count = recv(cdata->client_socket_fd, buf, BUFSZ, 0);
        if (count == 0)
        {
            printf("[log] %s disconnected\n", caddrstr);
            break;
        }
        buf[count] = '\0';
        if (!process_message(cdata, buf, count))
        {
            printf("[log] %s sent invalid message and was disconnected\n", caddrstr);
            break;
        }
        // replace \n with \0
        printf("[msg] %s, %d bytes: ---%s---\n", caddrstr, (int)count, buf);
    }

    close(cdata->client_socket_fd);

    cdata->sdata->clients_data.erase(std::remove(cdata->sdata->clients_data.begin(), cdata->sdata->clients_data.end(), cdata), cdata->sdata->clients_data.end());

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

    int socket_fd = socket(storage.ss_family, SOCK_STREAM, 0);
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
    printf("[log] bound to %s, waiting connections\n", addrstr);
    server_data sdata;
    sdata.socket_fd = socket_fd;

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

        struct client_data *cdata = (struct client_data *)malloc(sizeof(*cdata));
        if (!cdata)
        {
            logexit("malloc");
        }
        cdata->client_socket_fd = client_socket_fd;
        memcpy(&(cdata->storage), &client_storage, sizeof(client_storage));

        cdata->sdata = &sdata;
        sdata.clients_data.push_back(cdata);

        pthread_create(&cdata->tid, NULL, client_thread, cdata);
    }
}
