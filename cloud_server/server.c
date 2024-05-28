#include "server.h"

#define SERVER_PORT 8080
int upload(int socket, Client client);
int cloud_function(int socket);

int main(int argc, char *argv[])
{
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    struct sigaction sa;

    // SIGCHLD 핸들러 설정
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SERVER_PORT);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket < 0)
    {
        perror("socket");
        return -1;
    }

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("bind");
        return -1;
    }

    if (listen(server_socket, 5) < 0)
    {
        perror("listen");
        return -1;
    }

    while (1)
    {
        socklen_t client_addr_len = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket < 0)
        {
            perror("accept");
            return -1;
        }

        if (fork() == 0)
        {
            close(server_socket);
            cloud_function(client_socket);
            close(client_socket);
            exit(0);
        }
        close(client_socket);
    }
}

int cloud_function(int client_socket)
{

    Client client;
    int pid = getpid();
    printf("%d : 프로세스 클라우드 서버 시작\n", pid);
    while (1)
    {
        if (recv(client_socket, &client, sizeof(client), 0) < 0)
        {
            perror("recv");
            return -1;
        }
        if (!strcmp(client.command, "UPLOAD") || !strcmp(client.command, "upload"))
        {
            upload(client_socket, client);
        }
        else
        {
            exit(0);
        }
    }
}

int upload(int client_socket, Client client)
{
    FILE *file;
    password_tf password;
    char buffer[1024];
    int file_size = 0;
    int received = 0;
    int remain_data = 0;

    if (access(client.dir, F_OK) == -1)
    {
        if (create_directory(client) == -1)
        {
            return -1;
        }
    }
    if (check_password(client) == -1)
    {
        strncpy(password.tf, "incorrect", sizeof("incorrect"));
        send(client_socket, &password, sizeof(password), 0);
        return -1;
    }
    else
    {
        strncpy(password.tf, "correct", sizeof("correct"));
        send(client_socket, &password, sizeof(password), 0);
    }

    strcat(client.dir, "/");
    strcat(client.dir, client.filename);
    file = fopen(client.dir, "wb");
    if (file == NULL)
    {
        perror("fopen");
        return -1;
    }

    while (1)
    {
        if ((received = recv(client_socket, buffer, 1024, 0)) < 0)
        {
            perror("recv");
            return -1;
        }
        file_size += received;
        fwrite(buffer, 1, received, file);
        if (received < 1024)
        {
            break;
        }
    }

    printf("%d : %s 업로드 완료\n", getpid(), client.filename);

    fclose(file);
}
