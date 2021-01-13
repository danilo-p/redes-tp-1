#pragma once

#include <stdlib.h>

#include <arpa/inet.h>

void logexit(const char *msg);

void addrtostr(const struct sockaddr *addr, char *str, size_t strsize);
