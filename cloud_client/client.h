#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <libgen.h>
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>

#define BUF_SIZE 1024

typedef struct
{
    char command[10];
    char dir[100];
    char password[100];
    char filename[100];
} Client;

typedef struct
{
    char message[100];
} Message;

typedef struct
{
    char chat_message[BUF_SIZE];
    char name[20];
} Chatting;

char name[20] = "[DEFAULT]";

int client_cloud_function(int client_socket);
int upload(int client_socket, Client client);
int download(int client_socket, Client client);
int list(int client_socket, Client client);
void *recv_msg(void *arg);
void *send_msg(void *arg);
void chat(int client_socket);

int exist_file(char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
    {
        return -1;
    }
    fclose(fp);
    return 0;
}

void get_char(void)
{
    getchar();
    getchar();
}

void user_interface(Client *client)
{
    if (strcmp(client->command, "upload") == 0)
    {
        printf("업로드할 디렉토리 위치를 입력해주세요: ");
        scanf("%s", client->dir);
    }
    else if (strcmp(client->command, "download") == 0)
    {
        printf("다운로드할 디렉토리 위치를 입력해주세요: ");
        scanf("%s", client->dir);
    }
    printf("디렉토리 비밀번호를 입력해주세요: ");
    scanf("%s", client->password);
    printf("파일명을 입력해주세요: ");
    scanf("%s", client->filename);
}

void *send_msg(void *arg)
{
    int client_socket = *((int *)arg);
    Chatting chatting_recv;
    Chatting chatting_send;

    while (1)
    {
        fgets(chatting_recv.chat_message, BUF_SIZE, stdin);

        if (!strcmp(chatting_recv.chat_message, "q\n") || !strcmp(chatting_recv.chat_message, "Q\n"))
        {
            strcpy(chatting_send.chat_message, "STOP_CHAT");
            write(client_socket, &chatting_send, sizeof(chatting_send));
            pthread_exit(NULL);
        }

        strcpy(chatting_send.chat_message, chatting_recv.chat_message);
        strcpy(chatting_send.name, name);

        write(client_socket, &chatting_send, sizeof(chatting_send));
        memset(chatting_send.chat_message, 0, sizeof(chatting_send.chat_message));
        memset(chatting_recv.chat_message, 0, sizeof(chatting_recv.chat_message));
    }
    return NULL;
}

void *recv_msg(void *arg)
{
    int client_socket = *((int *)arg);
    Chatting chatting_recv;
    int str_len;

    while (1)
    {
        str_len = read(client_socket, &chatting_recv, sizeof(chatting_recv));

        if (str_len == -1)
            return (void *)-1;

        if (!strcmp(chatting_recv.chat_message, "STOP_CHAT"))
        {
            printf("상대방이 채팅을 종료했습니다.\n");
            pthread_exit(NULL);
        }
        chatting_recv.chat_message[str_len] = 0;
        fputs(chatting_recv.name, stdout);
        fputs(" : ", stdout);
        fputs(chatting_recv.chat_message, stdout);
        memset(chatting_recv.chat_message, 0, sizeof(chatting_recv.chat_message));
    }
    return NULL;
}