#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#include "client_ipv6.h"

#define PORT "22317"
#define MAX_PENDING 5
#define ADDRESS "www.he.net"

int main(){

    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    struct addrinfo hints, *res;
    int sock;
    char receive_buffer[256];
    int bytes_received;

    memset(&hints, 0, sizeof hints); // Initializam structura hints pentru getaddrinfo - cu 0
    hints.ai_family = AF_INET; // Specificam ca dorim adrese IPV4
    hints.ai_socktype = SOCK_STREAM; // Specificam ca dorim socket de tip stream (TCP)
    hints.ai_flags = AI_PASSIVE; // Specificam ca dorim sa ascultam pe toate interfetele disponibile 

    if(getaddrinfo(NULL, PORT, &hints, &res) != 0){
        fprintf(stderr, "Eroare la getaddrinfo: %s\n", gai_strerror(errno));
        return 1;
    }

    sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol); // Cream socketul
    if(sock == -1){
        fprintf(stderr, "Eroare la crearea socketului: %s\n", strerror(errno));
        return 1;
    }

    if(bind(sock, res->ai_addr, res->ai_addrlen) == -1){ // Facem bind la socket (asociem socketul cu adresa si portul)
        fprintf(stderr, "Eroare la bind: %s\n", strerror(errno));
        return 1;
    }

    if(listen(sock, MAX_PENDING) == -1){ // Punem socketul in stare de ascultare
        fprintf(stderr, "Eroare la listen: %s\n", strerror(errno));
        return 1;
    }
    printf("Serverul asculta pe portul %s...\n", PORT);
    addr_size = sizeof their_addr;
    
    while(1){
        int new_fd = accept(sock, (struct sockaddr *)&their_addr, &addr_size); // Acceptam o conexiune noua de la un client
        if(new_fd == -1){ // Verificam daca accept a avut loc succes
            fprintf(stderr, "Eroare la accept: %s\n", strerror(errno));
            continue;
        }
        getnameinfo((struct sockaddr *)&their_addr, addr_size, 
                    receive_buffer, sizeof receive_buffer, 
                    NULL, 0, NI_NUMERICHOST);
        printf("S-a acceptat o conexiune noua de la adresa %s\n", receive_buffer);
        
        pid_t pid = fork(); // Cream un proces copil pentru a gestiona conexiunea cu clientul
        if(pid == -1){ // Verificam daca fork a avut loc succes
            fprintf(stderr, "Eroare la fork: %s\n", strerror(errno));
            close(new_fd);
            continue;
        }
        if(pid == 0){ // Procesul copil gestioneaza conexiunea cu clientul
            close(sock); // Inchidem socketul principal in procesul copil
            while(1){ // Bucla pentru a primi si procesa date de la client
                bytes_received = recv(new_fd, receive_buffer, sizeof receive_buffer - 1, 0); // Primim date de la client
                if (bytes_received > 0) { // Verificam daca recv a avut succes
                    receive_buffer[bytes_received] = '\0'; // Adaugam terminatorul de sir
                    printf("Am primit de la client: %s\n", receive_buffer); // Afisam datele primite
                    if(strcmp(receive_buffer, "07#") == 0){ // Daca comanda este 07#
                        char *response = NULL; 
                        int response_size = 0;
                        if(get_page(ADDRESS, &response, &response_size) == 0){ // Obtinem pagina de la serverul web folosind functia din client_ipv6.c
                            send(new_fd, response, response_size, 0); // Trimitem raspunsul inapoi la client
                            free(response); // Eliberam memoria alocata pentru raspuns
                        } else {
                            char *error_msg = "Eroare la obtinerea paginii.\n";
                            send(new_fd, error_msg, strlen(error_msg), 0); // Trimitem mesaj de eroare la client daca obtinerea paginii a esuat
                        }
                    }
                    else{
                        char *msg = "Comanda necunoscuta.\n";
                        send(new_fd, msg, strlen(msg), 0); // Trimitem mesaj de eroare la client daca comanda este necunoscuta
                    }
                } else if (bytes_received == 0) {
                    printf("Clientul a închis conexiunea.\n"); // Clientul a închis conexiunea
                    break;
                } else {
                    perror("Eroare la recv"); 
                }
            }
            
            close(new_fd);
            printf("Conexiunea cu clientul a fost terminată.\n");
            printf("---------------------------------------------------------------\n");
            exit(0);
        }
    }

    freeaddrinfo(res);
    return 0;
}
