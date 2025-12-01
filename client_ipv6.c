#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

int get_page(char *address, char **response, int *response_size){
    
    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method()); // Initializam contextul SSL (ca si client)
    SSL *ssl = SSL_new(ctx); // Cream o noua structura SSL folosind contextul creat anterior

    int status;
    struct addrinfo hints;
    struct addrinfo *res;
    char host[250];
    char *request = "GET / HTTP/1.0\r\n\r\n";
    int len = strlen(request);
    int bytes_sent = 0;
    int bytes_received = 0;
    size_t total_size = 0;
    char buffer[4];
    *response = NULL;
    *response_size = 0;
    size_t body_size = 0;
    size_t header_size = 0;
    

	
    memset(&hints, 0, sizeof hints); // Initializam structura hints pentru getaddrinfo - cu 0
    hints.ai_family = AF_INET6; // Specificam ca dorim adrese IPV6
    hints.ai_socktype = SOCK_STREAM; // Specificam ca dorim socket de tip stream (TCP)
    printf("Obtinem adresa IPV6 a server-ului %s...\n", address); 
    status = getaddrinfo(address, "https", &hints, &res); // Obtinem adresa IPV6 a server-ului
    if(status != 0){ // Verificam daca getaddrinfo a avut succes
        printf("Error: %s", gai_strerror(status));
        return -1;
    }

    /**
     * getnameinfo:
     * - res->ai_addr: adresa socketului rezultată din rezolvarea numelui (struct sockaddr) care va fi convertită.
     * - res->ai_addrlen: lungimea structurii res->ai_addr, necesară pentru a limita citirea corectă a memoriei.
     * - host: bufferul unde va fi scrisă reprezentarea textuală (numerică) a adresei IP obținute.
     * - sizeof host: dimensiunea bufferului host, pentru a preveni depășirile de memorie.
     * - NULL: pointer pentru numele portului; NULL indică faptul că nu dorim să obținem această informație.
     * - 0: lungimea bufferului destinat numelui portului, nefolosit deoarece cel anterior este NULL.
     * - NI_NUMERICHOST: flag care specifică faptul că dorim adresa în formă numerică (fără rezolvare inversă DNS).
     */
    getnameinfo(res->ai_addr, res->ai_addrlen, host, sizeof host, NULL, 0, NI_NUMERICHOST); // Convertim adresa obtinuta intr-un string 
    printf("Adresa IPV6 a serverului %s este: %s\n", address, host);
    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol); // Cream socketul
    if(sock == -1 ){
        fprintf(stderr, "Eroarea la crearea socketului: %s\n", strerror(errno));
        freeaddrinfo(res); // Eliberam memoria alocata pentru adresa
        return -1;
    }
    printf("Socketul a fost creat cu succes!\n");

    int conn = connect(sock, res->ai_addr, res->ai_addrlen); // Ne conectam la server
    if(conn == -1){
        fprintf(stderr, "Eroare la conectare: %s\n", strerror(errno));
        close(sock);
        freeaddrinfo(res); // Eliberam memoria alocata pentru adresa
        return -1;
    }
    printf("Conexiunea a fost stabilita!\n");

    SSL_set_fd(ssl, sock); // Asociem socketul cu structura SSL

	if(SSL_connect(ssl) == -1){
        fprintf(stderr, "Eroare la SSL_connect: %s\n", ERR_error_string(SSL_get_error(ssl, -1), NULL));
        close(sock);
        freeaddrinfo(res);
        return -1;
    }


    bytes_sent = SSL_write(ssl, request, len); // Trimitem cererea HTTP catre server - cu SSL_write, care cripteaza datele inainte de trimitere
    if(bytes_sent == -1){
        fprintf(stderr, "Eroare la trimiterea datelor: %s\n", strerror(errno));
        close(sock);
        freeaddrinfo(res);
        return -1;
    }
    printf("S-au trimis %d bytes catre server\n", bytes_sent);
    while((bytes_received = SSL_read(ssl, buffer, sizeof buffer - 1)) > 0){ // Primim raspunsul de la server - cu SSL_read, care decripteaza datele primite
        char *new_resp = realloc(*response, total_size + bytes_received + 1); // Realocam memoria pentru raspunsul complet
        if(!new_resp){
            printf("Eroarea la alocarea memoriei\n");
            close(sock);
            freeaddrinfo(res);
            return -1;
        }
        *response = new_resp;
        memcpy(*response + total_size, buffer, bytes_received); // Copiem datele primite in raspunsul complet
        total_size += bytes_received; // Actualizam dimensiunea totala a raspunsului
        (*response)[total_size] = '\0'; // Adaugam terminatorul de sir
    }
    FILE *file = fopen("index.html", "w"); // Deschidem fisierul pentru scriere
    if(file == NULL){
        fprintf(stderr, "Eroare la deschiderea fisierului: %s\n", strerror(errno));
        close(sock);
        freeaddrinfo(res); // Eliberam memoria alocata pentru adresa
        return -1;
    }
        
	char *html_body = strstr(*response, "\r\n\r\n"); // Cautam inceputul corpului HTML in raspuns
	if(html_body != NULL){
		html_body += 4; // Sarim peste delimitatorul dintre header si body
		header_size = html_body - *response;// Calculam dimensiunea header-ului
		body_size = total_size - header_size;// Calculam dimensiunea corpului HTML
        fwrite(html_body, 1, body_size, file); // Scriem corpul HTML in fisier
        fclose(file); // Inchidem fisierul
        printf("Pagina salvata in index.html\n");
    }
    memmove(*response, html_body, body_size); // Mutam corpul HTML la inceputul raspunsului
    *response_size = body_size; // Actualizam dimensiunea raspunsului
    printf("S-au primit %zu bytes de la server\n", body_size);
    close(sock); // Inchidem socketul
    freeaddrinfo(res); // Eliberam memoria alocata pentru adresa
    SSL_free(ssl); // Eliberam structura SSL
    SSL_CTX_free(ctx); // Eliberam contextul SSL
    return 0;
}


