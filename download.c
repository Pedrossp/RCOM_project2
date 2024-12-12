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

#define RESPONSE_PATTERN "^([0-9]{3})"

// Estrutura para armazenar os dados do URL
typedef struct {
    char user[BUFFER_SIZE];
    char pass[BUFFER_SIZE];
    char host[BUFFER_SIZE];
    char path[BUFFER_SIZE];
    char file[BUFFER_SIZE]; 
} FTPUrl;

// Definição de enum para os estados
typedef enum {
    INIT,       // Estado inicial
    LINE_ONLY,  // Resposta de uma única linha
    MULTI_LINE, // Resposta com múltiplas linhas
    FINISHED    // Resposta finalizada
} ReadState;

// Função para ler as respostas do servidor FTP
int fetchResponse(const int socket, char* buffer) {
    char byte;
    int index = 0, responseCode;
    ReadState state = INIT;

    memset(buffer, 0, BUFFER_SIZE);

    while (state != FINISHED) {
        // Lê um byte do socket
        ssize_t bytesRead = read(socket, &byte, 1);
        
        if (bytesRead <= 0) {
            // Se não foi possível ler ou chegou ao fim da conexão
            perror("Erro ao ler do socket");
            return -1; // Erro
        }

        // Processa o byte lido de acordo com o estado
        switch (state) {
            case INIT:
                if (byte == ' ') {
                    state = LINE_ONLY;  // Quando encontra um espaço, vai para o estado de linha única
                } else if (byte == '-') {
                    state = MULTI_LINE; // Quando encontra um '-', entra no estado de múltiplas linhas
                } else if (byte == '\n') {
                    state = FINISHED;   // Linha vazia, finaliza a leitura
                } else {
                    buffer[index++] = byte; // Acumula o código de resposta na inicialização
                }
                break;
            case LINE_ONLY:
                if (byte == '\n') {
                    state = FINISHED;   // Quando encontra o fim de linha, termina
                } else {
                    buffer[index++] = byte; // Acumula a linha
                }
                break;
            case MULTI_LINE:
                if (byte == '\n') {
                    // Limpa o buffer após cada linha em uma resposta multi-linha
                    memset(buffer, 0, BUFFER_SIZE);
                    state = INIT;  // Retorna ao estado de inicialização
                    index = 0;
                } else {
                    buffer[index++] = byte; // Acumula o conteúdo da linha
                }
                break;
            case FINISHED:
                break;
            default:
                break;
        }
    }

    // Extraímos o código de resposta (código numérico de 3 dígitos)
    sscanf(buffer, "%d", &responseCode);

    return responseCode;  // Retorna o código de resposta
}


int parse_url(const char *url, FTPUrl *ftp_data) {
    // Inicializar valores padrão
    strcpy(ftp_data->user, DEFAULT_USER);
    strcpy(ftp_data->pass, DEFAULT_PASS);

    // Verificar se o URL contém credenciais
    const char *credentials_end = strchr(url, '@');
    const char *path_start;

    if (credentials_end) {
        // Extrair credenciais
        const char *credentials_start = url + strlen(FTP_PREFIX);
        size_t credentials_length = credentials_end - credentials_start;

        char credentials[BUFFER_SIZE];
        strncpy(credentials, credentials_start, credentials_length);
        credentials[credentials_length] = '\0';

        // Separar usuário e senha
        char *colon_pos = strchr(credentials, ':');
        if (colon_pos) {
            *colon_pos = '\0';
            strcpy(ftp_data->user, credentials);
            strcpy(ftp_data->pass, colon_pos + 1);
        } else {
            strcpy(ftp_data->user, credentials); // Apenas usuário
        }

        path_start = credentials_end + 1;
    } else {
        path_start = url + strlen(FTP_PREFIX);
    }

    // Extrair host e caminho
    const char *host_end = strchr(path_start, '/');
    if (!host_end) {
        fprintf(stderr, "URL inválido: caminho não encontrado.\n");
        return -1;
    }

    strncpy(ftp_data->host, path_start, host_end - path_start);
    ftp_data->host[host_end - path_start] = '\0';

    // Caminho completo (após o host)
    strcpy(ftp_data->path, host_end + 1);

    // Extrair o nome do arquivo do caminho
    const char *filename = strrchr(ftp_data->path, '/');
    if (filename) {
        strcpy(ftp_data->file, filename + 1); // Ignorar '/'
    } else {
        strcpy(ftp_data->file, ftp_data->path); // Caso seja apenas o arquivo
    }

    return 0;
}


char *resolve_ip(const char *hostname) {

    struct hostent *h;
    h = gethostbyname(hostname);

    if (h == NULL || h->h_addr_list[0] == NULL) {
        herror("gethostbyname");
        return NULL;  // Ou algum outro erro apropriado
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

int authenticate(int control_socket, const char *user, const char *password) {
    char buffer[BUFFER_SIZE];
    
    // Enviar o comando USER
    snprintf(buffer, sizeof(buffer), "USER %s\r\n", user);
    write(control_socket, buffer, strlen(buffer));

    // Ler a resposta do servidor
    int response_code = fetchResponse(control_socket, buffer);
    printf("Resposta após comando USER: %s\n", buffer);  // Exibe a resposta para depuração

    // Verificar se a resposta foi 331 (usuário aceito, pedindo senha)
    if (response_code != 331) {
        fprintf(stderr, "Erro: Falha no envio do usuário. Código de resposta: %d\n", response_code);
        return -1;
    }

    // Enviar o comando PASS
    snprintf(buffer, sizeof(buffer), "PASS %s\r\n", password);
    write(control_socket, buffer, strlen(buffer));

    // Ler a resposta do servidor
    response_code = fetchResponse(control_socket, buffer);
    printf("Resposta após comando PASS: %s\n", buffer);  // Exibe a resposta para depuração

    // Verificar se a resposta foi 230 (autenticação bem-sucedida)
    if (response_code != 230) {
        fprintf(stderr, "Erro: Falha na autenticação. Código de resposta: %d\n", response_code);
        return -1;
    }

    return 0;
}

int enter_passive_mode(int control_socket, char *data_ip, int *data_port) {
    char buffer[BUFFER_SIZE];
    int response_code;

    // Enviar o comando PASV para o servidor
    write(control_socket, "PASV\r\n", 6);

    // Ler a resposta do servidor usando a função fetchResponse
    response_code = fetchResponse(control_socket, buffer);
    if (response_code != 227) {
        fprintf(stderr, "Erro: Não foi possível entrar no modo passivo. Código de resposta: %d\n", response_code);
        return -1;
    }

    // Analisar a resposta para obter o IP e a porta
    int ip1, ip2, ip3, ip4, p1, p2;
    if (sscanf(strchr(buffer, '('), "(%d,%d,%d,%d,%d,%d)", &ip1, &ip2, &ip3, &ip4, &p1, &p2) != 6) {
        fprintf(stderr, "Erro: Formato de resposta PASV inválido.\n");
        return -1;
    }

    // Construir o IP e a porta a partir dos valores extraídos
    snprintf(data_ip, BUFFER_SIZE, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);
    *data_port = p1 * 256 + p2;

    return 0;
}

// Função para solicitar o arquivo ao servidor
int request_file(int control_socket, const char *path) {
    char buffer[BUFFER_SIZE];
    
    // Enviar o comando RETR para o servidor solicitando o arquivo
    snprintf(buffer, sizeof(buffer), "RETR %s\r\n", path);
    write(control_socket, buffer, strlen(buffer));
    
    // Ler a resposta do servidor usando a função fetchResponse
    int response_code = fetchResponse(control_socket, buffer);
    
    // Verificar se a resposta foi 150 (Arquivo pronto para transferência)
    if (response_code != 150) {
        fprintf(stderr, "Erro: Falha ao solicitar arquivo. Código de resposta: %d\n", response_code);
        return -1;
    }

    return 0;
}

// Função para transferir o arquivo do servidor para o cliente
int transfer_file(int control_socket, int data_socket, const char *filename) {
    printf("Tentando abrir o arquivo: %s\n", filename);
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("fopen()");
        return -1;
    }

    char buffer[BUFFER_SIZE];
    int bytes;
    char response[BUFFER_SIZE];

    // Ler os dados do socket de dados e escrever no arquivo
    while ((bytes = read(data_socket, buffer, sizeof(buffer))) > 0) {
        // Escrever os dados lidos no arquivo
        fwrite(buffer, 1, bytes, file);
    }

    // Fechar o arquivo após a transferência
    fclose(file);

    // Verificar se o número de bytes lidos é consistente
    if (bytes < 0) {
        perror("Erro ao ler do socket de dados");
        return -1;
    }

    // Agora, após a transferência, devemos verificar a resposta do servidor via control_socket
    int response_code = fetchResponse(control_socket, response);
    
    // Verificar se o código de resposta é 226 (Transferência de arquivo bem-sucedida)
    if (response_code != 226) {
        fprintf(stderr, "Erro: Resposta inesperada do servidor: %s\n", response);
        return -1;
    }

    printf("Transferência de arquivo concluída com sucesso.\n");
    return 0;
}

int close_connections(const int control_socket, const int data_socket) {
    char response[BUFFER_SIZE];

    // Envia o comando QUIT para o servidor, desconectando a ligação de controlo
    write(control_socket, "QUIT\r\n", 6);

    // Utiliza a função fetchResponse para ler a resposta
    if (fetchResponse(control_socket, response) != 221) {
        fprintf(stderr, "Erro: Falha na desconexão da ligação de controlo.\n");
        return -1;
    }

    // Fecha os dois sockets (controlo e dados)
    if (close(control_socket) == -1) {
        perror("Erro ao fechar o socket de controlo");
        return -1;
    }

    if (close(data_socket) == -1) {
        perror("Erro ao fechar o socket de dados");
        return -1;
    }

    return 0;  // As ligações foram fechadas com sucesso
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

     // 4. Ler a resposta de boas-vindas do servidor
    char response[BUFFER_SIZE];
    int response_code = fetchResponse(control_sockfd, response);
    if (response_code == 220) {
        printf("Conexão bem-sucedida: %s\n", response);
    } else {
        fprintf(stderr, "Erro na resposta de conexão: %s\n", response);
        close(control_sockfd);
        return -1;
    }

    // 5. Autenticar no servidor FTP
    if (authenticate(control_sockfd, ftp_data.user, ftp_data.pass) != 0) {
        close(control_sockfd);
        return -1;
    }

    // 6. Entrar no modo passivo
    char data_ip[BUFFER_SIZE];
    int data_port;
    if (enter_passive_mode(control_sockfd, data_ip, &data_port) != 0) {
        close(control_sockfd);
        return -1;
    }

    printf("Modo passivo: IP %s, Porta %d\n", data_ip, data_port);

    // 7. Conectar ao servidor de dados (modo passivo)
    int data_sockfd = connect_to_server(data_ip, data_port);

    // 8. Solicitar o arquivo do servidor
    if (request_file(control_sockfd, ftp_data.path) != 0) {
        close(control_sockfd);
        close(data_sockfd);
        return -1;
    }

    // 9. Transferir o arquivo do servidor para o cliente
    if (transfer_file(control_sockfd,data_sockfd, ftp_data.file) != 0) {
        close(control_sockfd);
        close(data_sockfd);
        return -1;
    }

    printf("Arquivo transferido com sucesso: %s\n", ftp_data.path);

    // 10. Fechar as conexões (controle e dados)
    if (close_connections(control_sockfd, data_sockfd) != 0) {
        return -1;
    }

    printf("Conexões fechadas com sucesso.\n");

    return 0;
}


