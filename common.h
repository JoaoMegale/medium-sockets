#pragma once

#include <stdlib.h>

#include <arpa/inet.h>

#define MAX_CLIENTS 10

struct BlogOperation {
    int client_id;
    int operation_type;
    int server_response;
    char topic[50];
    char content[2048];
};

struct Topic {
    char nome[50];
    int sub_clients[MAX_CLIENTS];
    int num_subs;
};

void logexit(const char *msg);

int addrparse(const char *addrstr, const char *portstr,
              struct sockaddr_storage *storage);

void addrtostr(const struct sockaddr *addr, char *str, size_t strsize);

int server_sockaddr_init(const char *proto, const char *portstr,
                         struct sockaddr_storage *storage);
