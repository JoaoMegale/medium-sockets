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

	struct BlogOperation connection;
	connection.client_id = 0;
	connection.operation_type = 1;
	connection.server_response = 0;
	strcpy(connection.topic, "");
	strcpy(connection.content, "");

	send(s, &connection, sizeof(connection), 0);


	printf("connected to %s\n", addrstr);

	struct BlogOperation operation;
	char client_input[BUFSZ];

	while(1) {
		printf("> ");
		fgets(client_input, BUFSZ-1, stdin);

		operation.server_response = 0;

		// publicar algo
		if (strncmp(client_input, "publish in ", 11) == 0) {
			operation.operation_type = 2;

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
	}
	close(s);

	exit(EXIT_SUCCESS);
}