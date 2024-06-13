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
} Message; // 메시지 구조체, 오류/성공 메시지 수신

typedef struct
{
    char chat_message[BUF_SIZE];
    char name[20];
} Chatting; // 채팅 구조체, 채팅 메시지 전달

char name[20] = "[DEFAULT]";

int client_cloud_function(int client_socket);
int upload(int client_socket, Client client);
int download(int client_socket, Client client);
int list(int client_socket, Client client);
void *recv_msg(void *arg);
void *send_msg(void *arg);
void chat(int client_socket);

// 조건 변수와 종료 플래그
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutx = PTHREAD_MUTEX_INITIALIZER; // 뮤텍스 변수 초기화

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

/*
 *유저 인터페이스 함수
 */
void user_interface(Client *client)
{
    if (strcmp(client->command, "upload") == 0 || strcmp(client->command, "UPLOAD") == 0)
    {
        printf("업로드할 디렉토리 위치를 입력해주세요: ");
        if (scanf("%s", client->dir) == EOF)
        {
            perror("scanf");
            exit(1);
        }
        printf("디렉토리 비밀번호를 입력해주세요: ");
        if (scanf("%s", client->password) == EOF)
        {
            perror("scanf");
            exit(1);
        }
        printf("파일명을 입력해주세요: ");
        if (scanf("%s", client->filename) == EOF)
        {
            perror("scanf");
            exit(1);
        }
    }
    else if (strcmp(client->command, "download") == 0 || strcmp(client->command, "DOWNLOAD") == 0)
    {
        printf("다운로드할 디렉토리 위치를 입력해주세요: ");
        if (scanf("%s", client->dir) == EOF)
        {
            perror("scanf");
            exit(1);
        }
        printf("디렉토리 비밀번호를 입력해주세요: ");
        if (scanf("%s", client->password) == EOF)
        {
            perror("scanf");
            exit(1);
        }
        printf("파일명을 입력해주세요: ");
        if (scanf("%s", client->filename) == EOF)
        {
            perror("scanf");
            exit(1);
        }
    }
    else if (strcmp(client->command, "list") == 0 || strcmp(client->command, "LIST") == 0)
    {
        printf("파일 목록을 조회할 디렉토리 위치를 입력해주세요: ");
        if (scanf("%s", client->dir) == EOF)
        {
            perror("scanf");
            exit(1);
        }
        printf("디렉토리 비밀번호를 입력해주세요: ");
        if (scanf("%s", client->password) == EOF)
        {
            perror("scanf");
            exit(1);
        }
    }
    else if (strcmp(client->command, "chat") == 0 || strcmp(client->command, "CHAT") == 0)
    {
        printf("채팅을 시작할 디렉토리(채팅방) 위치를 입력해주세요: ");
        if (scanf("%s", client->dir) == EOF)
        {
            perror("scanf");
            exit(1);
        }
        printf("디렉토리 비밀번호를 입력해주세요: ");
        if (scanf("%s", client->password) == EOF)
        {
            perror("scanf");
            exit(1);
        }

        printf("사용자 이름을 입력해주세요: ");
        if (scanf("%s", name) == EOF)
        {
            perror("scanf");
            exit(1);
        }
        while (getchar() != '\n') // 버퍼 비우기
            ;
    }
}

void *send_msg(void *arg) // 메시지 전송 함수
{
    int client_socket = *((int *)arg);
    Chatting chatting_recv;
    Chatting chatting_send;

    while (1)
    {
        if (fgets(chatting_recv.chat_message, BUF_SIZE, stdin) == NULL)
        {
            return (void *)-1;
        }

        if (!strcmp(chatting_recv.chat_message, "q\n") || !strcmp(chatting_recv.chat_message, "Q\n"))
        {
            strcpy(chatting_send.chat_message, "STOP_CHAT");
            if (write(client_socket, &chatting_send, sizeof(chatting_send)) == -1)
            {
                perror("write() error");
            }

            pthread_exit(NULL);
        }

        strcpy(chatting_send.chat_message, chatting_recv.chat_message);
        strcpy(chatting_send.name, name);

        if (write(client_socket, &chatting_send, sizeof(chatting_send)) == -1)
        {
            perror("write() error");
        }
        memset(chatting_send.chat_message, 0, sizeof(chatting_send.chat_message));
        memset(chatting_recv.chat_message, 0, sizeof(chatting_recv.chat_message));
    }
    return NULL;
}

void *recv_msg(void *arg) // 메시지 수신 함수
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
            strcpy(chatting_recv.chat_message, "상대방이 채팅을 종료하였습니다.\n");
            fputs(chatting_recv.chat_message, stdout);
        }
        else if (strcmp(chatting_recv.chat_message, "q") == 0)
        {
            pthread_exit(NULL);
        }
        else
        {
            chatting_recv.chat_message[str_len] = 0;
            fputs(chatting_recv.name, stdout);
            fputs(" : ", stdout);
            fputs(chatting_recv.chat_message, stdout);
        }
        memset(chatting_recv.chat_message, 0, sizeof(chatting_recv.chat_message));
    }
    return NULL;
}