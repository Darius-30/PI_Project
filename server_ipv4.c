#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define PORT "22317"
#define MAX_PENDING 5
#define ADDRESS "www.he.net"

int get_page(char *address, char **response, int *response_size);

int main(){


    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    struct addrinfo hints, *res;
    int sock;
    char receive_buffer[256];
    int bytes_received;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if(getaddrinfo(NULL, PORT, &hints, &res) != 0){
        fprintf(stderr, "Eroare la getaddrinfo: %s\n", gai_strerror(errno));
        return 1;
    }

    sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if(sock == -1){
        fprintf(stderr, "Eroare la crearea socketului: %s\n", strerror(errno));
        return 1;
    }

    if(bind(sock, res->ai_addr, res->ai_addrlen) == -1){
        fprintf(stderr, "Eroare la bind: %s\n", strerror(errno));
        return 1;
    }

    if(listen(sock, MAX_PENDING) == -1){
        fprintf(stderr, "Eroare la listen: %s\n", strerror(errno));
        return 1;
    }
    printf("Serverul asculta pe portul %s...\n", PORT);
    addr_size = sizeof their_addr;

    while(1){
        int new_fd = accept(sock, (struct sockaddr *)&their_addr, &addr_size);
        if(new_fd == -1){
            fprintf(stderr, "Eroare la accept: %s\n", strerror(errno));
            continue;
        }
        printf("S-a acceptat o conexiune noua de la adresa \n");
        while(1){
            bytes_received = recv(new_fd, receive_buffer, sizeof receive_buffer - 1, 0);
            if (bytes_received > 0) {
                receive_buffer[bytes_received] = '\0';
                printf("Am primit de la client: %s\n", receive_buffer);
                if(strcmp(receive_buffer, "07#") == 0){
                    char *response = NULL;
                    int response_size = 0;
                    if(get_page(ADDRESS, &response, &response_size) == 0){
                        send(new_fd, response, response_size, 0);
                        free(response);
                    } else {
                        char *error_msg = "Eroare la obtinerea paginii.\n";
                        send(new_fd, error_msg, strlen(error_msg), 0);
                    }
                }
                else{
                    char *msg = "Comanda necunoscuta.\n";
                    send(new_fd, msg, strlen(msg), 0);
                }
            } else if (bytes_received == 0) {
                printf("Clientul a închis conexiunea.\n");
                break;
            } else {
                perror("Eroare la recv");
            }
        }
        
        close(new_fd);
        printf("Conexiunea cu clientul a fost terminată.\n");
        printf("---------------------------------------------------------------\n");
    }

    freeaddrinfo(res);
    return 0;
}

int get_page(char *address, char **response, int *response_size){
    
    SSL_library_init();
    
    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    SSL *ssl = SSL_new(ctx);

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
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    printf("Obtinem adresa IPV6 a server-ului %s...\n", address);
    status = getaddrinfo(address, "https", &hints, &res);
    if(status != 0){
        printf("Error: %s", gai_strerror(status));
        return -1;
    }
    getnameinfo(res->ai_addr, res->ai_addrlen, host, sizeof host, NULL, 0, NI_NUMERICHOST);
    printf("Adresa IPV6 a serverului %s este: %s\n", address, host);
    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if(sock == -1 ){
        fprintf(stderr, "Eroarea la crearea socketului: %s\n", strerror(errno));
        freeaddrinfo(res);
        return -1;
    }
    printf("Socketul a fost creat cu succes!\n");
    int conn = connect(sock, res->ai_addr, res->ai_addrlen);
    if(conn == -1){
        fprintf(stderr, "Eroare la conectare: %s\n", strerror(errno));
        close(sock);
        freeaddrinfo(res);
        return -1;
    }
    printf("Conexiunea a fost stabilita!\n");
    SSL_set_fd(ssl, sock);

    if(SSL_connect(ssl) == -1){
        fprintf(stderr, "Eroare la SSL_connect: %s\n", ERR_error_string(SSL_get_error(ssl, -1), NULL));
        close(sock);
        freeaddrinfo(res);
        return -1;
    }
    printf("Conexiunea SSL a fost stabilita cu succes!\n");
    bytes_sent = SSL_write(ssl, request, len);
    if(bytes_sent == -1){
        fprintf(stderr, "Eroare la trimiterea datelor: %s\n", strerror(errno));
        close(sock);
        freeaddrinfo(res);
        return -1;
    }
    printf("S-au trimis %d bytes catre server\n", bytes_sent);
    while((bytes_received = SSL_read(ssl, buffer, sizeof buffer - 1)) > 0){
        char *new_resp = realloc(*response, total_size + bytes_received + 1);
        if(!new_resp){
            printf("Eroarea la alocarea memoriei\n");
            break;
            close(sock);
            freeaddrinfo(res);
            return -1;
        }
        *response = new_resp;
        memcpy(*response + total_size, buffer, bytes_received);
        total_size += bytes_received;
        (*response)[total_size] = '\0';
    }
    FILE *file = fopen("index.html", "w");
    if(file == NULL){
        fprintf(stderr, "Eroare la deschiderea fisierului: %s\n", strerror(errno));
        close(sock);
        freeaddrinfo(res);
        return -1;
    }
        
	char *html_body = strstr(*response, "\r\n\r\n");
	if(html_body != NULL){
		html_body += 4;
		header_size = html_body - *response;
		body_size = total_size - header_size;
        fwrite(html_body, 1, body_size, file);
        fclose(file);
        printf("Pagina salvata in index.html\n");
    }
    memmove(*response, html_body, body_size);
    *response_size = body_size;
    printf("S-au primit %zu bytes de la server\n", body_size);
    close(sock);
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    freeaddrinfo(res);
    return 0;
}