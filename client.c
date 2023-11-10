#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

void usage(int argc, char **argv) {
	printf("usage: %s <server IP> <server port>\n", argv[0]);
	printf("example: %s 127.0.0.1 51511\n", argv[0]);
	exit(EXIT_FAILURE);
}

#define BUFSZ 1024

int main(int argc, char **argv) {
	if (argc < 3) {
		usage(argc, argv);
	}

	struct sockaddr_storage storage;
	if (0 != addrparse(argv[1], argv[2], &storage)) {
		usage(argc, argv);
	}

	int s;
	s = socket(storage.ss_family, SOCK_STREAM, 0);
	if (s == -1) {
		logexit("socket");
	}
	struct sockaddr *addr = (struct sockaddr *)(&storage);
	if (0 != connect(s, addr, sizeof(storage))) {
		logexit("connect");
	}

	char addrstr[BUFSZ];
	addrtostr(addr, addrstr, BUFSZ);

	int client_id = 0;

	struct BlogOperation connection;
	connection.client_id = 0;
	connection.operation_type = 1;
	connection.server_response = 0;
	strcpy(connection.topic, "");
	strcpy(connection.content, "");

	send(s, &connection, sizeof(connection), 0);

	struct BlogOperation id_attribution;

	ssize_t bytes_received = recv(s, &id_attribution, sizeof(id_attribution), 0);
	if (bytes_received == -1) {
		logexit("recieve");
	} else if (bytes_received == 0) { // conexão terminada
		close(s);
	}
	else {
		client_id = id_attribution.client_id;
	}
	printf("id atribuido: %d\n", client_id);


	printf("connected to %s\n", addrstr);

	struct BlogOperation operation;
	char client_input[BUFSZ];

	while(1) {
		printf("> ");
		fgets(client_input, BUFSZ-1, stdin);

		operation.server_response = 0;
		operation.client_id = client_id;

		// publicar algo
		if (strncmp(client_input, "publish in ", 11) == 0) {
			operation.operation_type = 2;

			printf("> ");
			client_input[strcspn(client_input, "\n")] = 0;
			strncpy(operation.topic, client_input + 11, sizeof(operation.topic));

			memset(operation.content, 0, sizeof(operation.content));

			fgets(client_input, sizeof(client_input), stdin);

			client_input[strcspn(client_input, "\n")] = 0;
			strncpy(operation.content, client_input, sizeof(operation.content));
		}

		// listar topicos
		else if ((strcmp(client_input, "list topics\n") == 0)) {
			operation.operation_type = 3;
		}

		// inscrição em um tópico
		else if (strncmp(client_input, "subscribe ", 10) == 0) {
			operation.operation_type = 4;

			client_input[strcspn(client_input, "\n")] = 0;
			strncpy(operation.topic, client_input + 10, sizeof(operation.topic));
		}

		// desconectar
		else if (strcmp(client_input, "exit\n") == 0) {
			operation.operation_type = 5;
			send(s, &operation, sizeof(operation), 0);
			break;
		}

		// desinscrição em um tópico
		else if (strncmp(client_input, "unsubscribe ", 12) == 0) {
			operation.operation_type = 6;

			client_input[strcspn(client_input, "\n")] = 0;
			strncpy(operation.topic, client_input + 12, sizeof(operation.topic));
		}

		else {
			printf("comando desconhecido.\n");
		}
		
		send(s, &operation, sizeof(operation), 0);

		struct BlogOperation server_resp;

		ssize_t bytes_received = recv(s, &server_resp, sizeof(server_resp), 0);
		if (bytes_received == -1) {
			logexit("recieve");
		} else if (bytes_received == 0) { // conexão terminada
			break;
		}

		if (server_resp.operation_type == 1) {
			client_id = server_resp.client_id;
		}

	}
	close(s);

	exit(EXIT_SUCCESS);
}