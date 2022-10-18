#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include <stdbool.h>

#define MAX_EQUIPMENTS 15
#define BUFFER_SZ 2048

/* Variaveis globais */
static _Atomic unsigned int eqpCount = 0;
static int uId = 1;

/* Vetores criados para controle interno dos equipamentos, nao sendo relacionados a criacao de thread */
bool equipmentExists[MAX_EQUIPMENTS];
float equipmentData[MAX_EQUIPMENTS];

/* Struct com os parametros do equipamento */
typedef struct{
	struct sockaddr_in address;
	int sockfd;
	int id;
	int registro;
} equipment_t;

/* Vetor criado para controle dos equipamentos e threads a serem manipulados */
equipment_t *equipmentReference[MAX_EQUIPMENTS];

pthread_mutex_t equipmentReference_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Funcao que retorna uma mensagem de erro de acordo com o codigo de erro passado como parametro de entrada */
char* setErrorMessage(int errorId){
	if(errorId == 1) return ("Equipment not found\n");
	else if(errorId == 2) return ("Source equipment not found\n");
	else if(errorId == 3) return ("Target equipment not found\n");
	else if(errorId == 4) return ("Equipment limit exceeded\n");
	return ("Unknown command\n");
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

/* Funcao adiciona o equipamento dado como parametro de entrada a fila */
void addEquipment(equipment_t *eqp){
	pthread_mutex_lock(&equipmentReference_mutex);
	for(int i=0; i<MAX_EQUIPMENTS; i++){
		if(!equipmentReference[i]){
			equipmentReference[i] = eqp;
			break;
		}
	}
	pthread_mutex_unlock(&equipmentReference_mutex);
}

/* Funcao remove o equipamento dado como parametro de entrada da fila */
void removeEquipment(int equipmentId){
	pthread_mutex_lock(&equipmentReference_mutex);
	for(int i=0; i<MAX_EQUIPMENTS; i++){
		if(equipmentReference[i]){
			if(equipmentReference[i]->id == equipmentId){
				equipmentReference[i] = NULL;
				break;
			}
		}
	}
	pthread_mutex_unlock(&equipmentReference_mutex);
}

/* Funcao altera o vetor equipmentExists de acordo com os parametros recebidos. Eh utilizada para controle interno dos equipamentos existentes, nao sendo relacionada a criacao de thread */
int equipmentChange(int change, int size, int id){
	if(change != 0){
		for(int i=0; i<=(size+1); i++){
			if(equipmentExists[i] == false){
				equipmentExists[i] = true;
				return(i+1);
			}
		}
	}
	equipmentExists[id-1] = false;
	return 0;
}

/* Funcao retorna uma string contendo todos os equipamentos presentes na base de dados */
const char* listEquipments(){
	char *output = malloc (sizeof (char) * 65);
	char* aux = malloc (sizeof (char) * 5);
	for(int i=0; i<MAX_EQUIPMENTS; i++){
		if(equipmentExists[i]==true){
			if(i==0) sprintf(aux, " 0%d", i+1); 
			strcat(output, aux);
		}
	}
	return output;	
}

/* Funcao envia mensagem do equipamento solicitante para ele mesmo */
void sendMessageToHimself(char *message, int equipmentId){
	pthread_mutex_lock(&equipmentReference_mutex);
	if(write(equipmentReference[equipmentId]->sockfd, message, strlen(message)) < 0){ perror("write to himself failed"); }
	pthread_mutex_unlock(&equipmentReference_mutex);
}

/* Funcao envia mensagem do equipamento solicitante para todos os outros conectados */
void sendMessageToTheOthers(char *message, int equipmentId){
	pthread_mutex_lock(&equipmentReference_mutex);
	for(int i=0; i<MAX_EQUIPMENTS; i++){
		if(equipmentReference[i]){
			if(equipmentReference[i]->id != equipmentId){
				if(write(equipmentReference[i]->sockfd, message, strlen(message)) < 0){
					perror("write to the others failed");
					break;
				}
			}
		}
	}
	pthread_mutex_unlock(&equipmentReference_mutex);
}

/* Funcao configura e envia as mensagens com as informacoes do equipamento */
void equipmentInformations(char* message, int equipmentReg, int equipmentId){
	int i = 0;
	char* parse = malloc(sizeof(char)*100);
	char *command[50];
	char *fullCommand = strtok(message," ");

	while (fullCommand != NULL){
		command[i++] = fullCommand;
		fullCommand = strtok(NULL, " ");
	}

	if(command[3]!=NULL){ 
		int aux = atoi(command[3])-1;
		if (aux<MAX_EQUIPMENTS){
			if(equipmentExists[aux]==true){
				sendMessageToHimself("requested information\n", aux);
				if((aux+1) < 10) sprintf(parse,"Value from 0%d: %.2f\n", aux+1, equipmentData[aux]);
				else sprintf(parse,"Value from %d: %.2f\n", aux+1, equipmentData[aux]);
				printf("Equipamento %d solicita informação para equipamento %d na rede\n", (equipmentId), aux+1);
				sendMessageToHimself(parse, equipmentReg);
			}
			else{
				sendMessageToHimself(setErrorMessage(3), equipmentReg);
				printf("Equipment %s not found\n", (command[3]));
			}
		}
		else{
			sendMessageToHimself(setErrorMessage(3), equipmentReg);
			printf("Equipment %s not found\n", (command[3]));
		}
	}
}

/* Funcao opera toda a parte de comunicacao entre o server e o equipamento */
void *equipCommOperations(void *arg){
	char outputBuffer[BUFFER_SZ];
	int closeConnection = 0;

	eqpCount++;
	equipment_t *Equipment = (equipment_t *)arg;
	
	if(Equipment->registro < 10) sprintf(outputBuffer, "New ID: 0%d\n", Equipment->registro);
	else sprintf(outputBuffer, "New ID: %d\n", Equipment->registro);
	sendMessageToHimself(outputBuffer, Equipment->registro-1);
	bzero(outputBuffer, BUFFER_SZ);

	if(Equipment->registro < 10) sprintf(outputBuffer, "Equipment 0%d added\n", Equipment->registro);
	else sprintf(outputBuffer, "Equipment %d added\n", Equipment->registro);
	printf("%s", outputBuffer);
	sendMessageToTheOthers(outputBuffer, Equipment->id);
	bzero(outputBuffer, BUFFER_SZ);

	while(1){
		if(closeConnection){
			break;
		}

		int receive = recv(Equipment->sockfd, outputBuffer, BUFFER_SZ, 0);
		if (receive > 0){
			if(strlen(outputBuffer) > 0){
				if(strcmp(outputBuffer, "list equipment") == 0){
					sprintf(outputBuffer, "%s\n", listEquipments());
					sendMessageToHimself(outputBuffer, Equipment->registro-1);
				}
				else if((strcmp(outputBuffer, "request information from 01") == 0) || (strcmp(outputBuffer, "request information from 02") == 0) || (strcmp(outputBuffer, "request information from 03") == 0) || (strcmp(outputBuffer, "request information from 04") == 0) || (strcmp(outputBuffer, "request information from 05") == 0) || (strcmp(outputBuffer, "request information from 06") == 0) || (strcmp(outputBuffer, "request information from 07") == 0) || (strcmp(outputBuffer, "request information from 08") == 0) || (strcmp(outputBuffer, "request information from 09") == 0) || (strcmp(outputBuffer, "request information from 10") == 0) || (strcmp(outputBuffer, "request information from 11") == 0) || (strcmp(outputBuffer, "request information from 12") == 0) || (strcmp(outputBuffer, "request information from 13") == 0) || (strcmp(outputBuffer, "request information from 14") == 0) || (strcmp(outputBuffer, "request information from 15") == 0)){
					equipmentInformations(outputBuffer, Equipment->registro-1, Equipment->id);
					trimString(outputBuffer, strlen(outputBuffer));
				}
			}
		}else if (receive == 0 || strcmp(outputBuffer, "close connection") == 0){
			if(Equipment->registro < 10) sprintf(outputBuffer, "Equipment 0%d removed\n", Equipment->registro);
			else sprintf(outputBuffer, "Equipment %d removed\n", Equipment->registro);
			equipmentChange(0,0, Equipment->registro);
			printf("%s", outputBuffer);
			sendMessageToTheOthers(outputBuffer, Equipment->id);
			closeConnection = 1;
		} 
		else {
			printf("ERROR: -1\n");
			closeConnection = 1;
		}

		bzero(outputBuffer, BUFFER_SZ);
	}

  	/* Deleta o equipamento da thread, finaliza a comunicacao dele com o servidor (port, socket, afins) e libera o espaco na thread */
	close(Equipment->sockfd);
    removeEquipment(Equipment->id);
    free(Equipment);
    eqpCount--;
    pthread_detach(pthread_self());

	return NULL;
}

int main(int argc, char **argv){
	if(argc != 2){
		printf("Usage: %s <port>\n", argv[0]);
		return EXIT_FAILURE;
	}

	char *ip = "127.0.0.1";
	int port = atoi(argv[1]);
	int connFd = 0;
	struct sockaddr_in serv_addr;
	struct sockaddr_in eqp_addr;
	pthread_t tId;

	/* Configuracoes do socket */
	int sock = socket(AF_INET, SOCK_STREAM, 0);

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(ip);
	serv_addr.sin_port = htons(port);

	signal(SIGPIPE, SIG_IGN);

	if (sock == -1) {
        perror("socket");
		return EXIT_FAILURE;
    }

	int opt = 1;
	if(setsockopt(sock, SOL_SOCKET,(SO_REUSEPORT | SO_REUSEADDR),(char*)&opt,sizeof(opt)) < 0){
		perror("setsockopt failed");
    	return EXIT_FAILURE;
	}

	if(bind(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		perror("bind failed");
		return EXIT_FAILURE;
	}

	if (listen(sock, 10) < 0) {
		perror("listen failed");
		return EXIT_FAILURE;
	}

	while(1){
		socklen_t eqpLen = sizeof(eqp_addr);
		connFd = accept(sock, (struct sockaddr*)&eqp_addr, &eqpLen);
		srand(time(NULL));
		
		if((eqpCount) == MAX_EQUIPMENTS){
			printf("%s", setErrorMessage(4));
			close(connFd);
			continue;
		}

		/* Configuracoes do equipamento */
		equipment_t *eqp = (equipment_t *)malloc(sizeof(equipment_t));
		eqp->address = eqp_addr;
		eqp->sockfd = connFd;
		eqp->id = uId++;
		eqp->registro = equipmentChange(1,eqpCount,0);
		equipmentData[eqp->registro-1]=((rand() % 100)/10);

		/* Adiciona equipamento a fila e cria a thread desse equipamento */
		addEquipment(eqp);
		pthread_create(&tId, NULL, &equipCommOperations, (void*)eqp);
	}

	return EXIT_SUCCESS;
}