#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define LENGTH 2048

/* Variaveis globais */
volatile sig_atomic_t closeConnection = 0;
int sock = 0;

/* Funcao adiciona os caracteres '> ' entre um comando e outro, usada apenas para fins de facilitar o envio de comandos pelo teclado */
void strOverwriteStdout() {
	printf("%s", "> ");
	fflush(stdout);
}

/* Funcao itera sobre a string, caractere por caractere, incrementando o ponteiro de 1 em 1, ate encontrar o caractere '\n' para substitui-lo por '\0' */
void trimString(char* inputData, int dataSize) {
	for(int i = 0; i<dataSize; i++) {
		if(inputData[i] == '\n'){
			inputData[i] = '\0';
			break;
		}
	}
}

/* Funcao armazena os ultimos dados trocados */
void catchDataAndExit(int sig) {
    closeConnection = 1;
}

/* Funcao opera a parte de envio de mensagens */
void sendingDataOperations() {
	char message[LENGTH] = {};
	char buffer[LENGTH + 34] = {};

	while(1) {
		strOverwriteStdout();
		fgets(message, LENGTH, stdin);
		trimString(message, LENGTH);

		if (strcmp(message, "close connection") == 0) {
			break;
		}
		else {
			sprintf(buffer, "%s", message);
			send(sock, buffer, strlen(buffer), 0);
		}

		bzero(message, LENGTH);
		bzero(buffer, LENGTH + 32);
	}
	catchDataAndExit(2);
}

/* Funcao opera a parte de recebimento de mensagens */
void receivingDataOperations() {
	char message[LENGTH] = {};
	while (1) {
		int receive = recv(sock, message, LENGTH, 0);
		if (receive > 0) {
			printf("%s", message);
			strOverwriteStdout();
		}
		else if (receive == 0) {
			break;
		}
		else {
			//-1
		}
		memset(message, 0, sizeof(message));
	}
}

int main(int argc, char **argv){
	if(argc != 3){
		printf("Usage: %s <ip address> <port>\n", argv[0]);
		return EXIT_FAILURE;
	}

	char *ip =  "127.0.0.1";
	int port = atoi(argv[2]);

	signal(SIGINT, catchDataAndExit);

	struct sockaddr_in server_addr;

	/* Configuracoes do socket */
	sock = socket(AF_INET, SOCK_STREAM, 0);
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(ip);
	server_addr.sin_port = htons(port);

	int connFd = connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
	if (connFd == -1) {
		perror("connect failed");
		return EXIT_FAILURE;
	}

	pthread_t sendMsgThread;
	if(pthread_create(&sendMsgThread, NULL, (void *) sendingDataOperations, NULL) != 0){
		perror("pthread to send failed");
    	return EXIT_FAILURE;
	}

	pthread_t recvMsgThread;
  	if(pthread_create(&recvMsgThread, NULL, (void *) receivingDataOperations, NULL) != 0){
		perror("pthread to recv failed");
		return EXIT_FAILURE;
	}

	while (1){
		if(closeConnection){
		printf("\nFechamento de Conexão com Servidor\n");
		printf("\nSuccessful removal\n");
		break;
    	}
	}

	close(sock);

	return EXIT_SUCCESS;
}