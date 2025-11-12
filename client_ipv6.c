#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>


int main(){
	char *address = "www.he.net";
	int status;
	struct addrinfo hints;
	struct addrinfo *res;
	char host[250];
	char *request = "GET / HTTP/1.1\r\nHost: www.he.net\r\nConnection: close\r\n\r\n";
	int len = strlen(request);
	int bytes_sent = 0;
	int bytes_received = 0;
	char *response = NULL;
	size_t total_size = 0;
	char buffer[4096];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET; //AF_INET6;
	hints.ai_socktype = SOCK_STREAM;

	printf("Obtinem adresa IPV6 a server-ului %s...\n", address);

	if(getaddrinfo(address, "http", &hints, &res) != 0){
		printf("Error: %s", gai_strerror(status));
		return 1;
	}

	getnameinfo(res->ai_addr, res->ai_addrlen, host, sizeof host, NULL, 0, NI_NUMERICHOST);
	printf("Adresa IPV6 a serverului %s este: %s\n", address, host);

	int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol); 
	if(sock == -1 ){
		fprintf(stderr, "Eroarea la crearea socketului: %s\n", strerror(errno));
		return 1;
	}
	printf("Socketul a fost creat cu succes!\n");

	int conn = connect(sock, res->ai_addr, res->ai_addrlen);
	if(conn == -1){
		fprintf(stderr, "Eroare la conectare: %s\n", strerror(errno));
		return 1;
	}
	printf("Conexiunea a fost stabilita!\n");

	bytes_sent = send(sock, request, len, 0);
	if(bytes_sent == -1){
		fprintf(stderr, "Eroare la trimiterea datelor: %s\n",
			strerror(errno));
		return 1;
	}
	printf("S-au trimis %d bytes catre server\n", bytes_sent);

	while((bytes_received = recv(sock, buffer, sizeof buffer - 1, 0)) > 0){
		char *new_resp = realloc(response, total_size + bytes_received + 1);
		if(!new_resp){
			printf("Eroarea la alocarea memoriei\n");
			break;
		}
		response = new_resp;
		memcpy(response + total_size, buffer, bytes_received);
		total_size += bytes_received;
		response[total_size] = '\0';
	}
	
	if(bytes_received == -1){
		fprintf(stderr, "Eroare la primirea datelor: %s\n",
			strerror(errno));
		return 1;
	}

	FILE *file = fopen("index.html", "w");
	if(file == NULL){
		fprintf(stderr, "Eroare la deschiderea fisierului: %s\n",strerror(errno));
		return 1;
	}
	fwrite(response, 1, total_size, file);
	printf("Raspunsul a fost salvat in fisierul index.html si contine %d bytes\n", total_size);

	
	free(response);
	fclose(file);	
	freeaddrinfo(res);
	close(sock);
	return 0;
}


