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
} Message; // 메시지 구조체, 오류/성공 메시지 전달

typedef struct
{
    char chat_message[BUF_SIZE];
    char name[20];
} Chatting; // 채팅 구조체, 채팅 메시지 전달

typedef struct
{
    int clnt_sock;
    char dir[100]; // 클라이언트 소켓, 디렉토리 경로(채팅 방에 해당하는 디렉토리 경로)
} Chat_Setting;    // 채팅 설정 구조체, 채팅 설정 정보 전달

int clnt_cnt = 0;
int clnt_socks[MAX_CLNT];
pthread_mutex_t mutx = PTHREAD_MUTEX_INITIALIZER; // 뮤텍스 변수 초기화
Chat_Setting chat_clnt[MAX_CLNT];                 // 채팅 설정 구조체 배열

int upload(int socket, Client client);
int download(int client_socket, Client client);
int list(int client_socket, Client client);
void *cloud_function_thread(void *arg);
void send_msg(Chatting *chatting, int len, int clnt_sock, char *dir);
void *chat_client(void *arg);
void chat(int client_socket, Client client);

/*
 * 디렉토리 생성 함수
 */
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

/*
 * 비밀번호 확인 함수
 */
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
    if (fscanf(fp, "%s", password) == EOF)
    {
        printf("비밀번호가 일치하지 않습니다.\n");
        return -1;
    }
    fclose(fp);

    // 비밀번호 비교
    if (strcmp(password, client.password) != 0)
    {
        printf("비밀번호가 일치하지 않습니다.\n");
        return -1;
    }
    return 0;
}

/*
 * 디렉토리 존재 여부, 비밀번호 일치 여부 확인 함수
 */
int check_list_chat(int client_socket, Client client)
{
    Message msg;
    if (access(client.dir, F_OK) == -1) // 디렉토리가 존재하지 않으면
    {
        strncpy(msg.message, "not exist", sizeof("not exist"));
        send(client_socket, &msg, sizeof(msg), 0);
        return -1;
    }
    else if (check_password(client) == -1) // 비밀번호가 일치하지 않으면
    {
        strncpy(msg.message, "incorrect", sizeof("incorrect"));
        send(client_socket, &msg, sizeof(msg), 0);
        return -1;
    }
    else
    {
        strncpy(msg.message, "correct", sizeof("correct"));
        send(client_socket, &msg, sizeof(msg), 0);
    }
    return 0;
}

void *chat_client(void *arg)
{

    Chat_Setting chat_setting = *((Chat_Setting *)arg);

    int clnt_sock = chat_setting.clnt_sock; // 클라이언트 소켓
    char dir[100];                          // 채팅 방 디렉토리 경로
    strcpy(dir, chat_setting.dir);

    Chatting chatting;
    int str_len;

    printf("채팅을 시작합니다.\n");

    while ((str_len = read(clnt_sock, &chatting, sizeof(chatting))) != 0)
    {
        chatting.chat_message[str_len] = 0;
        if (strcmp(chatting.chat_message, "STOP_CHAT") == 0) // 채팅 종료
        {
            send_msg(&chatting, sizeof(chatting), clnt_sock, dir);
            break;
        }
        send_msg(&chatting, sizeof(chatting), clnt_sock, dir);
    }
    memset(chatting.chat_message, 0, sizeof(chatting.chat_message));

    pthread_mutex_lock(&mutx);
    for (int i = 0; i < clnt_cnt; i++) // 채팅 클라이언트 배열에서 삭제
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
        // 자기 자신을 제외하고, 같은 채팅 방에 있는 클라이언트에게만 메시지 전송
        if (clnt_sock != clnt_socks[i] && strcmp(chat_clnt[i].dir, dir) == 0)
        {
            if (write(clnt_socks[i], chatting, len) == -1)
            {
                perror("write() error");
            }
        }
        // 자기 자신의 recv 스레드를 종료하기 위한 메시지 전송
        if (clnt_sock == clnt_socks[i] && strcmp(chatting->chat_message, "STOP_CHAT") == 0)
        {
            strcpy(chatting->chat_message, "q");
            if (write(clnt_sock, chatting, len) == -1) // 자기 자신에게 메시지 전송
            {
                perror("write() error");
            }
        }
    }
    pthread_mutex_unlock(&mutx);
}