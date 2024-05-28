#include "client.h"

#define SERVER_PORT 8080

int client_cloud_function(int client_socket);
int upload(int client_socket, Client client);
int download(int client_socket, Client client);
int list(int client_socket, Client client);

int main(int argc, char *argv[])
{
    int client_socket;
    struct sockaddr_in server_addr;
    Client client;

    if (argc != 2)
    {
        printf("Usage: %s <server_ip>\n", argv[0]);
        return -1;
    }

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0)
    {
        perror("socket");
        return -1;
    }

    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);
    server_addr.sin_port = htons(SERVER_PORT);

    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connect");
        return -1;
    }

    printf("Connected to server\n");

    client_cloud_function(client_socket);

    return 0;
}

int client_cloud_function(int client_socket)
{
    Client client;
    Message msg;
    char buffer[1024];
    int n;

    while (1)
    {
        // 터미널 clear
        system("clear");

        printf("Enter command: ");
        scanf("%s", client.command);
        if (strcmp(client.command, "exit") == 0)
        {
            send(client_socket, &client, sizeof(client), 0);
            // 버퍼 비우기
            break;
        }
        else if (strcmp(client.command, "upload") == 0 || strcmp(client.command, "UPLOAD") == 0)
        {
            user_interface(&client);

            if (exist_file(client.filename) == -1)
            {
                printf("파일이 존재하지 않습니다.\n");
                get_char();
                continue;
            }

            send(client_socket, &client, sizeof(client), 0);

            recv(client_socket, &msg, sizeof(msg), 0);

            if (strncmp(msg.message, "correct", 7) == 0)
            {
                upload(client_socket, client);
                printf("업로드 완료\n");
            }
            else
            {
                printf("비밀번호가 틀렸습니다. 다시 입력해주세요.\n");
            }
            printf("Enter 키를 누르세요\n");
            get_char();
        }
        else if (strcmp(client.command, "download") == 0 || strcmp(client.command, "DOWNLOAD") == 0)
        {
            user_interface(&client);

            send(client_socket, &client, sizeof(client), 0);

            recv(client_socket, &msg, sizeof(msg), 0);

            if (strncmp(msg.message, "correct", 7) == 0)
            {
                download(client_socket, client);
                printf("다운로드 완료\n");
            }
            else if (strncmp(msg.message, "incorrect", 9) == 0)
            {
                printf("비밀번호가 틀렸습니다. 다시 입력해주세요.\n");
            }
            else
            {
                printf("파일 또는 디렉토리가 존재하지 않습니다.\n");
            }
            printf("Enter 키를 누르세요\n");
            get_char();
        }
        else if (strcmp(client.command, "list") == 0 || strcmp(client.command, "LIST") == 0)
        {
            printf("디렉토리 위치를 입력해주세요: ");
            scanf("%s", client.dir);
            printf("디렉토리 비밀번호를 입력해주세요: ");
            scanf("%s", client.password);

            send(client_socket, &client, sizeof(client), 0);

            recv(client_socket, &msg, sizeof(msg), 0);

            if (strncmp(msg.message, "correct", 7) == 0)
            {
                list(client_socket, client);
            }
            else if (strcmp(msg.message, "incorrect") == 0)
            {
                printf("비밀번호가 틀렸습니다. 다시 입력해주세요.\n");
            }
            else
            {
                printf("디렉토리가 존재하지 않습니다.\n");
            }
            printf("Enter 키를 누르세요\n");
            get_char();
        }
    }
}

int upload(int client_socket, Client client)
{
    char buffer[1024];
    char filename[100];
    int n;
    FILE *fp;

    fp = fopen(client.filename, "rb");
    if (fp == NULL)
    {
        perror("fopen");
        exit(0);
    }

    while (1)
    {
        n = fread(buffer, 1, sizeof(buffer), fp);
        if (n <= 0)
        {
            break;
        }
        send(client_socket, buffer, n, 0);
    }

    fclose(fp);

    return 0;
}

int download(int client_socket, Client client)
{
    FILE *fp;
    char buffer[1024];
    int received = 0;
    int file_size = 0;

    fp = fopen(client.filename, "wb");
    if (fp == NULL)
    {
        perror("fopen");
        exit(0);
    }

    while (1)
    {
        if ((received = recv(client_socket, buffer, 1024, 0)) < 0)
        {
            perror("recv");
            return -1;
        }
        file_size += received;
        fwrite(buffer, 1, received, fp);
        if (received < 1024)
        {
            break;
        }
    }

    printf("%s 다운로드 완료\n", client.filename);

    fclose(fp);

    return 0;
}

int list(int client_socket, Client client)
{

    char buffer[1024];
    int received = 0;

    // 서버로부터 파일 리스트를 받아와서 화면에 출력
    while (1)
    {
        if ((received = recv(client_socket, buffer, 1024, 0)) < 0)
        {
            perror("recv");
            return -1;
        }
        fwrite(buffer, 1, received, stdout);
        if (received < 1024)
        {
            break;
        }
    }

    return 0;
}
