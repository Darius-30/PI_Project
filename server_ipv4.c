#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>

#define PORT "22317"
#define MAX_PENDING 5

int main(){

    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    struct addrinfo hints, *res;
    int sock;
    char receive_buffer[1024];
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
    int new_fd = accept(sock, (struct sockaddr *)&their_addr, &addr_size);
    if(new_fd == -1){
        fprintf(stderr, "Eroare la accept: %s\n", strerror(errno));
        return 1;
    }
    printf("S-a acceptat o conexiune noua!\n");

    while(1){
        bytes_received = recv(new_fd, receive_buffer, sizeof receive_buffer - 1, 0);
        if (bytes_received > 0) {
            receive_buffer[bytes_received] = '\0';
            printf("Am primit de la client: %s\n", receive_buffer);
        } else if (bytes_received == 0) {
            printf("Clientul a închis conexiunea.\n");
            break;
        } else {
            perror("Eroare la recv");
        }
    }

    close(new_fd);
    close(sock);
    freeaddrinfo(res);
    return 0;
}