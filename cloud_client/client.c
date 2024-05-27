#include "client.h"

#define SERVER_PORT 8080

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
    char buffer[1024];
    int n;

    while (1)
    {
        printf("Enter command: ");
        scanf("%s", client.command);
        if (strcmp(client.command, "exit") == 0)
        {
            send(client_socket, &client, sizeof(client), 0);
            break;
        }
        else if (strcmp(client.command, "upload") == 0)
        {
            printf("Enter filename: ");
            scanf("%s", client.filename);
            send(client_socket, &client, sizeof(client), 0);
            upload(client_socket, client);
        }
    }
}

int upload(int client_socket, Client client)
{
    char buffer[1024];
    int n;
    FILE *fp;

    fp = fopen(client.filename, "r");
    if (fp == NULL)
    {
        perror("fopen");
        return -1;
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
