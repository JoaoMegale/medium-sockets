#pragma once

#include <stdlib.h>

#include <arpa/inet.h>

#define MAX_CLIENTS 10
#define MAX_TOPICS 10

struct client_data {
    int csock;
    struct sockaddr_storage storage;
    int id;
    struct server_data *sdata;
};

struct BlogOperation {
    int client_id;
    int operation_type;
    int server_response;
    char topic[50];
    char content[2048];
};

struct Topic {
    char nome[50];
    struct client_data sub_clients[MAX_CLIENTS];
    int num_subs;
};

struct server_data {
    struct Topic topicos[MAX_CLIENTS];
};

void logexit(const char *msg);

int addrparse(const char *addrstr, const char *portstr,
              struct sockaddr_storage *storage);

int server_sockaddr_init(const char *proto, const char *portstr,
                         struct sockaddr_storage *storage);
