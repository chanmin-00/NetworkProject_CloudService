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
#include <sys/file.h>
#include <semaphore.h>
#define MAX_CLNT 50
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

typedef struct
{
    int clnt_sock;
    char dir[100];
} Chat;

int clnt_cnt = 0;
int clnt_socks[MAX_CLNT];
pthread_mutex_t mutx = PTHREAD_MUTEX_INITIALIZER; // 뮤텍스 변수 초기화
Chat chat_clnt[MAX_CLNT];

int upload(int socket, Client client);
int download(int client_socket, Client client);
int list(int client_socket, Client client);
void *cloud_function_thread(void *arg);
void send_msg(Chatting *chatting, int len, int clnt_sock, char *dir);
void *chat_client(void *arg);
void chat(int client_socket, Client client);

void sigchld_handler(int s)
{
    // 종료되는 자식 프로세스의 pid 출력
    pid_t pid;
    int status;                                    // 자식 프로세스의 종료 상태
    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) // 모든 종료된 자식 프로세스에 대해
    {
        // 자식 프로세스의 pid와 종료 상태 출력
        printf("%d 종료 : 프로세스가 종료되었습니다\n", pid);
    }
}

int create_directory(Client client)
{
    char password_path[512];

    // client_group 디렉토리 생성
    if (mkdir(client.dir, 0777) == -1 && errno != EEXIST)
    {
        perror("create_directory : mkdir() error");
        return -1;
    }

    // 비밀번호 파일 경로 설정
    sprintf(password_path, "%s/password.txt", client.dir);

    // 비밀번호 파일 생성 및 비밀번호 기록
    FILE *fp = fopen(password_path, "w");
    if (fp == NULL)
    {
        perror("create_directory : password.txt fopen() error");
        return -1;
    }
    fprintf(fp, "%s\n", client.password);
    fclose(fp);

    // 비밀번호 파일 권한 설정
    if (chmod(password_path, 0600) == -1)
    {
        perror("create_directory : chmod() error");
        return -1;
    }

    return 0;
}

int check_password(Client client)
{
    char password_path[512];
    char password[100];

    // 비밀번호 파일 경로 설정
    sprintf(password_path, "%s/password.txt", client.dir);

    // 비밀번호 파일 읽기
    FILE *fp = fopen(password_path, "r");
    if (fp == NULL)
    {
        perror("check_password : password.txt fopen() error");
        return -1;
    }
    fscanf(fp, "%s", password);
    fclose(fp);

    // 비밀번호 비교
    if (strcmp(password, client.password) != 0)
    {
        printf("비밀번호가 일치하지 않습니다.\n");
        return -1;
    }
    return 0;
}

void *chat_client(void *arg)
{

    Chat chat = *((Chat *)arg);

    int clnt_sock = chat.clnt_sock;
    char dir[100];
    strcpy(dir, chat.dir);

    Chatting chatting;
    int str_len;

    printf("채팅을 시작합니다.\n");

    while ((str_len = read(clnt_sock, &chatting, sizeof(chatting))) != 0)
    {
        chatting.chat_message[str_len] = 0;
        if (strcmp(chatting.chat_message, "STOP_CHAT") == 0)
        {
            printf("상대방이 채팅을 종료했습니다.\n");
            write(clnt_sock, &chatting, sizeof(chatting));
            pthread_exit(NULL);
        }
        printf("%s\n", chatting.name);
        printf("%s\n", chatting.chat_message);
        send_msg(&chatting, sizeof(chatting), clnt_sock, dir);
        memset(chatting.chat_message, 0, sizeof(chatting.chat_message));
    }

    pthread_mutex_lock(&mutx);
    for (int i = 0; i < clnt_cnt; i++)
    {
        if (clnt_sock == clnt_socks[i])
        {
            while (i++ < clnt_cnt - 1)
            {
                clnt_socks[i] = clnt_socks[i + 1];
                chat_clnt[i] = chat_clnt[i + 1];
            }
            break;
        }
    }

    clnt_cnt--;
    pthread_mutex_unlock(&mutx);
    return NULL;
}

void send_msg(Chatting *chatting, int len, int clnt_sock, char *dir)
{
    int i;
    pthread_mutex_lock(&mutx);
    for (i = 0; i < clnt_cnt; i++)
    {
        if (clnt_sock != clnt_socks[i])
        {
            write(clnt_socks[i], chatting, len);
        }
    }
    pthread_mutex_unlock(&mutx);
}