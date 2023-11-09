#include "common.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>

#define BUFSZ 1024
#define MAX_CLIENTS 10

int ids_disponiveis[MAX_CLIENTS];

int get_prox_id() {
    // Retorne o próximo ID disponível e marque-o como usado
    if (ids_disponiveis[0] == 0) {
        // Todos os IDs estão em uso
        return -1; // Ou outra sinalização de erro, se desejar
    }

    int next_id = ids_disponiveis[0];
    // Remova o ID da lista (shift left)
    for (int i = 0; i < MAX_CLIENTS - 1; i++) {
        ids_disponiveis[i] = ids_disponiveis[i + 1];
    }
    ids_disponiveis[MAX_CLIENTS - 1] = 0; // Marque o último como vazio

    return next_id;
}

void liberar_id(int client_id) {

    int pos ;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (ids_disponiveis[i] >= client_id) {
            pos = i;
            break;
        } 
    }

    for (int i = pos+1; i <= MAX_CLIENTS; i++) {
        ids_disponiveis[i] = ids_disponiveis[i-1];
    }
    ids_disponiveis[pos] = client_id;
}

void usage(int argc, char **argv) {
    printf("usage: %s <v4|v6> <server port>\n", argv[0]);
    printf("example: %s v4 51511\n", argv[0]);
    exit(EXIT_FAILURE);
}

struct client_data {
    int csock;
    struct sockaddr_storage storage;
    int id;
};

void * client_thread(void *data) {
    struct client_data *cdata = (struct client_data *)data;
    struct sockaddr *caddr = (struct sockaddr *)(&cdata->storage);

    char caddrstr[BUFSZ];
    addrtostr(caddr, caddrstr, BUFSZ);

    while (1) {
        struct BlogOperation operation;
        memset(&operation, 0, sizeof(operation));
        size_t count = recv(cdata->csock, &operation, sizeof(operation), 0);

        struct BlogOperation server_resp;
        server_resp.server_response = 1;
        server_resp.operation_type = operation.operation_type;
        strcpy(server_resp.content, operation.content);
        strcpy(server_resp.topic, operation.topic);
        
        if ((int)count < 0) {
            perror("recv");
            break;
        } else if ((int)count == 0) {
            printf("client %d disconnected.\n", operation.client_id);
            break;
        } else if (operation.operation_type == 1) {
            printf("client %d connected\n", cdata->id);
            server_resp.client_id = cdata->id;
        } else if (operation.operation_type == 2) {
            printf("Publicado %s em %s by %d\n", operation.content, operation.topic, operation.client_id);
        } else if (operation.operation_type == 3) {
            printf("listagem de topicos\n");
        } else if (operation.operation_type == 4) {
            printf("client %d subscribed in %s\n", operation.client_id, operation.topic);
        } else if (operation.operation_type == 5) {
            printf("client %d disconnected.\n", operation.client_id);
            break;
        } else if (operation.operation_type == 6) {
            printf("client %d unsubscribed from %s\n", operation.client_id, operation.topic);
        } else {
            printf("comando desconhecido\n");
        }

        send(cdata->csock, &server_resp, sizeof(server_resp), 0);

    }

    close(cdata->csock);
    liberar_id(cdata->id);


    pthread_exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        usage(argc, argv);
    }

    struct sockaddr_storage storage;
    if (0 != server_sockaddr_init(argv[1], argv[2], &storage)) {
        usage(argc, argv);
    }

    for (int i = 1; i <= MAX_CLIENTS; i++) {
        ids_disponiveis[i - 1] = i;
    }

    int s;
    s = socket(storage.ss_family, SOCK_STREAM, 0);
    if (s == -1) {
        logexit("socket");
    }

    int enable = 1;
    if (0 != setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int))) {
        logexit("setsockopt");
    }

    struct sockaddr *addr = (struct sockaddr *)(&storage);
    if (0 != bind(s, addr, sizeof(storage))) {
        logexit("bind");
    }

    if (0 != listen(s, 10)) {
        logexit("listen");
    }

    char addrstr[BUFSZ];
    addrtostr(addr, addrstr, BUFSZ);
    printf("bound to %s, waiting connections\n", addrstr);

    while (1) {
        struct sockaddr_storage cstorage;
        struct sockaddr *caddr = (struct sockaddr *)(&cstorage);
        socklen_t caddrlen = sizeof(cstorage);

        int csock = accept(s, caddr, &caddrlen);
        if (csock == -1) {
            logexit("accept");
        }

        int client_id = get_prox_id();

        struct client_data *cdata = malloc(sizeof(*cdata));
        if (!cdata) {
            logexit("malloc");
        }
        cdata->csock = csock;
        memcpy(&(cdata->storage), &cstorage, sizeof(cstorage));
        cdata->id = client_id;

        pthread_t tid;
        pthread_create(&tid, NULL, client_thread, cdata);
    }

    exit(EXIT_SUCCESS);
}