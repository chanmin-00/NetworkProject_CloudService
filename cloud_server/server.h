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
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#define MAX_CLNT 50
#define BUF_SIZE 1024

#define CHK_NULL(x)  \
    if ((x) == NULL) \
        exit(1);
#define CHK_ERR(err, s) \
    if ((err) == -1)    \
    {                   \
        perror(s);      \
        exit(1);        \
    }
#define CHK_SSL(err)                 \
    if ((err) == -1)                 \
    {                                \
        ERR_print_errors_fp(stderr); \
        exit(2);                     \
    }

typedef struct
{
    char command[10];
    char dir[100];
    char password[100];
    char filename[100];
} Client; // 클라이언트 구조체, 클라이언트 명령어, 디렉토리 경로, 비밀번호, 파일 이름 전달

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
    SSL *ssl;
} Chat_Setting; // 채팅 설정 구조체, 채팅 설정 정보 전달

int clnt_cnt = 0;
int clnt_socks[MAX_CLNT];                         // 클라이언트 소켓 배열
pthread_mutex_t mutx = PTHREAD_MUTEX_INITIALIZER; // 뮤텍스 변수 초기화
Chat_Setting chat_clnt[MAX_CLNT];                 // 채팅 설정 구조체 배열

int upload(int socket, Client client, SSL *ssl);
int download(int client_socket, Client client, SSL *ssl);
int list(int client_socket, Client client, SSL *ssl);
void chat(int client_socket, Client client, SSL *ssl);
void *cloud_function_thread(void *arg);
void send_msg(Chatting *chatting, int len, int clnt_sock, char *dir, SSL *ssl);
void *chat_client(void *arg);

/*
 * verify_callback 함수, 인증서 검증 콜백 함수
 */
static int verify_callback(int preverify_ok, X509_STORE_CTX *ctx)
{

    char *str;
    X509 *cert = X509_STORE_CTX_get_current_cert(ctx); // 현재 인증서

    if (cert)
    {
        printf("Cert depth %d\n", X509_STORE_CTX_get_error_depth(ctx)); // 인증서 깊이

        str = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0); // 인증서의 subject
        CHK_NULL(str);
        printf("\t subject : %s\n", str);
        OPENSSL_free(str);

        str = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0); // 인증서의 issuer
        CHK_NULL(str);
        printf("\t issuer : %s\n", str);
        OPENSSL_free(str);
    }

    return preverify_ok;
}

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
 * 디렉토리 존재 여부 및 비밀번호 일치 여부 확인 함수
 */
int check_list_chat(int client_socket, Client client, SSL *ssl)
{
    Message msg;
    if (access(client.dir, F_OK) == -1) // 디렉토리가 존재하지 않으면
    {
        strncpy(msg.message, "not exist", sizeof("not exist"));
        SSL_write(ssl, &msg, sizeof(msg));
        return -1;
    }
    else if (check_password(client) == -1) // 비밀번호가 일치하지 않으면
    {
        strncpy(msg.message, "incorrect", sizeof("incorrect"));
        SSL_write(ssl, &msg, sizeof(msg));
        return -1;
    }
    else
    {
        strncpy(msg.message, "correct", sizeof("correct"));
        SSL_write(ssl, &msg, sizeof(msg));
    }
    return 0;
}

/*
 * 채팅 방 생성 함수, 같은 디렉토리를 공유하는 사람들끼리 파일 작업과 같은 내용 채팅을 가능하게 함
 */
void *chat_client(void *arg)
{
    Chat_Setting chat_setting = *((Chat_Setting *)arg);

    int clnt_sock = chat_setting.clnt_sock; // 클라이언트 소켓
    SSL *ssl = chat_setting.ssl;            // SSL 구조체
    char dir[100];                          // 채팅 방 디렉토리 경로
    strcpy(dir, chat_setting.dir);

    Chatting chatting; // 채팅 구조체
    int str_len;

    printf("채팅을 시작합니다.\n");
    while ((str_len = SSL_read(ssl, &chatting, sizeof(chatting))) != 0)
    {
        chatting.chat_message[str_len] = 0;
        if (strcmp(chatting.chat_message, "STOP_CHAT") == 0) // 채팅 종료
        {
            send_msg(&chatting, sizeof(chatting), clnt_sock, dir, ssl);
            break;
        }
        send_msg(&chatting, sizeof(chatting), clnt_sock, dir, ssl);
    }
    memset(chatting.chat_message, 0, sizeof(chatting.chat_message)); // 채팅 메시지 초기화

    pthread_mutex_lock(&mutx);         // 뮤텍스 락
    for (int i = 0; i < clnt_cnt; i++) // 채팅 클라이언트 배열에서 삭제, 채팅을 종료한 경우
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
    pthread_mutex_unlock(&mutx); // 뮤텍스 언락
    return NULL;
}

/*
 * 클라이언트에게 메시지 전송 함수
 */
void send_msg(Chatting *chatting, int len, int clnt_sock, char *dir, SSL *ssl)
{
    int i;
    pthread_mutex_lock(&mutx); // 뮤텍스 락
    for (i = 0; i < clnt_cnt; i++)
    {
        // 자기 자신을 제외하고, 같은 채팅 방에 있는 클라이언트에게만 메시지 전송
        if (clnt_sock != clnt_socks[i] && strcmp(chat_clnt[i].dir, dir) == 0)
        {
            SSL_write(chat_clnt[i].ssl, chatting, len);
        }
        // STOP_CHAT 메시지를 보낸 경우, 자기 자신의 recv 스레드를 종료하기 위한 메시지 전송
        if (clnt_sock == clnt_socks[i] && strcmp(chatting->chat_message, "STOP_CHAT") == 0)
        {
            strcpy(chatting->chat_message, "q");
            SSL_write(chat_clnt[i].ssl, chatting, len);
        }
    }
    pthread_mutex_unlock(&mutx); // 뮤텍스 언락
}