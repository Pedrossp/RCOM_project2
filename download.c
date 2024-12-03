#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>


#define FTP_PORT 21
#define BUFFER_SIZE 1024

#define DEFAULT_USER "anonymous"
#define DEFAULT_PASS "anonymous"
#define FTP_PREFIX "ftp://"

// Estrutura para armazenar os dados do URL
typedef struct {
    char user[BUFFER_SIZE];
    char pass[BUFFER_SIZE];
    char host[BUFFER_SIZE];
    char path[BUFFER_SIZE];
} FTPUrl;


int parse_url(const char *url, FTPUrl *ftp_data) { // exemplo -> ftp://anonymous:anonymous@ftp.bit.nl/speedtest/100mb.bin
    // Inicializar valores padrão
    strcpy(ftp_data->user, DEFAULT_USER);
    strcpy(ftp_data->pass, DEFAULT_PASS);

    // Verificar se o URL contém credenciais
    const char *credentials_end = strchr(url, '@');
    const char *path_start;

    if (credentials_end) {
        
        //ficar com anonymous:anonymous (entre ftp:// e @ ) -> são as credenciais
        const char *credentials_start = url + strlen(FTP_PREFIX); // salta o ftp://
        size_t credentials_length = credentials_end - credentials_start; // calcular o tamanho 

        char credentials[BUFFER_SIZE];
        strncpy(credentials, credentials_start, credentials_length);
        credentials[credentials_length] = '\0';

        // Separar user e password
        char *colon_pos = strchr(credentials, ':');
        if (colon_pos) {
            *colon_pos = '\0'; // Substituir ':' por '\0' para dividir a string
            strcpy(ftp_data->user, credentials);
            strcpy(ftp_data->pass, colon_pos + 1);
        } else {
            strcpy(ftp_data->user, credentials); // Apenas usuário, sem senha
        }

        path_start = credentials_end + 1;
    } else {
        path_start = url + strlen(FTP_PREFIX);
    }

    // Extrair host e caminho
    const char *host_end = strchr(path_start, '/');
    strncpy(ftp_data->host, path_start, host_end - path_start);
    ftp_data->host[host_end - path_start] = '\0';

    strcpy(ftp_data->path, host_end + 1);

    return 0;
}

char *resolve_ip(const char *hostname) {

    struct hostent *h;
    if (h = gethostbyname(hostname) == NULL) {
        herror("gethostbyname()");
        exit(-1);
    }

    return inet_ntoa(*((struct in_addr *) h->h_addr_list[0]));
}


// Função para criar e conectar o socket
int connect_to_server(const char *ip, int port) {

    int sockfd;
    struct sockaddr_in server_addr;

    /*server address handling*/
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);

    /*open a TCP socket*/
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }

    /*connect to the server*/
    if (connect(sockfd,
                (struct sockaddr *) &server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect()");
        exit(-1);
    }

    return sockfd;

}


int main(int argc, char *argv[]){
    
     if (argc != 2) {
        fprintf(stderr, "Uso: %s <url>\n", argv[0]);
        return -1;
    }


    FTPUrl ftp_data;
    const char *url = argv[1];

    // 1. Analisar URL
    if (parse_url(url, &ftp_data) != 0) {
        return -1;
    }

    printf("Servidor: %s, Ficheiro: %s, Utilizador: %s\n",
           ftp_data.host, ftp_data.path, ftp_data.user);


    // 2. IP do servidor
    char *server_ip = resolve_ip(ftp_data.host);

    // 3. Conectar ao servidor
    int control_sockfd = connect_to_server(server_ip, FTP_PORT);




    /* TESTAR URL_PARSE
    FTPUrl ftp_data;

    // Definir URLs para testar
    const char *urls[] = {
        "ftp://demo:password@test.rebex.net/readme.txt",
        "ftp://ftp.up.pt/pub/gnu/emacs/elisp-manual-21-2.8.tar.gz",
        "ftp://anonymous:anonymous@ftp.bit.nl/speedtest/100mb.bin"
    };

    // Testar cada URL
    for (int i = 0; i < 3; i++) {
        printf("URL: %s\n", urls[i]);
        if (parse_url(urls[i], &ftp_data) == 0) {
            // Exibir os dados extraídos
            printf("User: %s\n", ftp_data.user);
            printf("Pass: %s\n", ftp_data.pass);
            printf("Host: %s\n", ftp_data.host);
            printf("Path: %s\n", ftp_data.path);
        } else {
            printf("Erro ao processar a URL.\n");
        }
        printf("\n");
    }
    */


}
