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

    // Checa se há algum tópico disponível
    if (ids_disponiveis[0] == 0) {
        return -1;
    }

    // Primeiro elemento do array é o próximo
    int next_id = ids_disponiveis[0];

    // Remove o ID da lista (shift left)
    for (int i = 0; i < MAX_CLIENTS - 1; i++) {
        ids_disponiveis[i] = ids_disponiveis[i + 1];
    }
    ids_disponiveis[MAX_CLIENTS - 1] = 0;

    return next_id;
}

void liberar_id(int client_id) {

    // Procura a posição que o id liberado ficará
    int pos ;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (ids_disponiveis[i] >= client_id) {
            pos = i;
            break;
        } 
    }

    // Shift-right
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

void * client_thread(void *data) {
    struct client_data *cdata = (struct client_data *)data;

    while (1) {
        
        struct BlogOperation operation;
        memset(&operation, 0, sizeof(operation));
        size_t count = recv(cdata->csock, &operation, sizeof(operation), 0);

        // Dados padronizados para a resposta do servidor
        struct BlogOperation server_resp;
        server_resp.server_response = 1;
        server_resp.operation_type = operation.operation_type;
        strcpy(server_resp.topic, operation.topic);
        
        if ((int)count < 0) {
            perror("recv");
            break;
        } else if ((int)count == 0) {
            printf("client %02d disconnected.\n", operation.client_id);
            break;
        } 

        // Atribuição do ID do cliente
        else if (operation.operation_type == 1) {
            printf("client %02d connected\n", cdata->id);
            server_resp.client_id = cdata->id;
        }

        // Publish
        else if (operation.operation_type == 2) {
            printf("new post added in %s by %02d\n", operation.topic, operation.client_id);

            // Identifica o tópico ou cria um novo
            int topic_index = -1;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (strcmp(cdata->sdata->topicos[i].nome, operation.topic) == 0) {
                    topic_index = i;
                    break;
                } else if (strlen(cdata->sdata->topicos[i].nome) == 0) {
                    strcpy(cdata->sdata->topicos[i].nome, operation.topic);
                    topic_index = i;
                    break;
                }
            }

            // Enviar a mensagem para todos os clientes inscritos no tópico
            if (topic_index != -1) {
                for (int i = 0; i < cdata->sdata->topicos[topic_index].num_subs; i++) {
                    struct BlogOperation broadcast_msg;
                    broadcast_msg.client_id = operation.client_id;
                    broadcast_msg.operation_type = 2;
                    broadcast_msg.server_response = 1;
                    strcpy(broadcast_msg.topic, operation.topic);
                    strcpy(broadcast_msg.content, operation.content);

                    send(cdata->sdata->topicos[topic_index].sub_clients[i].csock, &broadcast_msg, sizeof(broadcast_msg), 0);
                }
            }
        }

        // List topics
        else if (operation.operation_type == 3) {

            int isEmpty = 1;

            // Armazena os tópicos em uma string, que será enviada como o conteúdo da resposta
            strcpy(server_resp.content, "");
            for (int i = 0; i < MAX_TOPICS; i++) {
                if (strlen(cdata->sdata->topicos[i].nome) != 0) {
                    strcat(server_resp.content, cdata->sdata->topicos[i].nome);
                    strcat(server_resp.content, "\n");
                    isEmpty = 0;
                }
            }

            // Checa se não há nenhum tópico ainda
            if (isEmpty == 1) {
                strcpy(server_resp.content, "no topics available\n");
            }

        } 

        // Subscribe
        else if (operation.operation_type == 4) {

            // Encontra o tópico ou cria um novo
            int topic_index = -1;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (strcmp(cdata->sdata->topicos[i].nome, operation.topic) == 0) {
                    topic_index = i;
                    break;
                } else if (strlen(cdata->sdata->topicos[i].nome) == 0) {
                    strcpy(cdata->sdata->topicos[i].nome, operation.topic);
                    topic_index = i;
                    break;
                }
            }

            // Checa se o cliente já está inscrito
            if (topic_index != -1 && cdata->sdata->topicos[topic_index].num_subs < MAX_CLIENTS) {
                int client_already_subscribed = 0;
                for (int i = 0; i < cdata->sdata->topicos[topic_index].num_subs; i++) {
                    if (cdata->sdata->topicos[topic_index].sub_clients[i].id == cdata->id) {
                        client_already_subscribed = 1;
                        break;
                    }
                }

                // Se não estiver inscrito, inscreve
                if (!client_already_subscribed) {
                    cdata->sdata->topicos[topic_index].sub_clients[cdata->sdata->topicos[topic_index].num_subs++].id = cdata->id;
                    printf("client %02d subscribed to %s\n", operation.client_id, operation.topic);
                }
                // Se estiver inscrito, envia mensagem de erro
                else {
                    strcat(server_resp.content, "error: already subscribed\n");
                }
            }

            // Adiciona o socket do cliente ao array do tópico, para que ele receba as mensagens
            cdata->sdata->topicos[topic_index].sub_clients[cdata->sdata->topicos[topic_index].num_subs-1].csock = cdata->csock;
        }

        // Exit
        else if (operation.operation_type == 5) {
            printf("client %02d disconnected.\n", operation.client_id);
            break;
        }

        // Unsubscribe
        else if (operation.operation_type == 6) {
            printf("client %02d unsubscribed from %s\n", operation.client_id, operation.topic);

            // Encontra o tópico
            int topic_index = -1;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (strcmp(cdata->sdata->topicos[i].nome, operation.topic) == 0) {
                    topic_index = i;
                    break;
                }
            }

            // Remove o cliente da lista de inscritos
            if (topic_index != -1) {
                for (int i = 0; i < cdata->sdata->topicos[topic_index].num_subs; i++) {
                    if (cdata->sdata->topicos[topic_index].sub_clients[i].id == cdata->id) {
                        // Shift left para remover o cliente da lista
                        for (int j = i; j < cdata->sdata->topicos[topic_index].num_subs - 1; j++) {
                            cdata->sdata->topicos[topic_index].sub_clients[j] = cdata->sdata->topicos[topic_index].sub_clients[j + 1];
                        }
                        cdata->sdata->topicos[topic_index].num_subs--;
                        break;
                    }
                }
            }
        } 

        else {
            printf("comando desconhecido\n");
        }

        // O servidor envia uma resposta ao cliente, a menos que a operação seja de publish (a mensagem será enviada a outros).
        if (server_resp.operation_type != 2) {
            send(cdata->csock, &server_resp, sizeof(server_resp), 0);
        }
        

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

    struct server_data sdata;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        strcpy(sdata.topicos[i].nome, "");
        sdata.topicos[i].num_subs = 0;
    }

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
        cdata->sdata = &sdata;

        pthread_t tid;
        pthread_create(&tid, NULL, client_thread, cdata);

    }

    exit(EXIT_SUCCESS);
}