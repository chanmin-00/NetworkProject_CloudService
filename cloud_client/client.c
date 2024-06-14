#include "client.h"

#define SERVER_PORT 8080

int main(int argc, char *argv[])
{
    int client_socket;
    struct sockaddr_in server_addr;
    Client client;

    SSL_CTX *ctx;             // SSL_CTX 구조체 포인터 선언
    SSL *ssl;                 // SSL 구조체 포인터 선언
    X509 *server_cert;        // 서버 인증서 구조체 포인터 선언
    const SSL_METHOD *method; // SSL 메소드 구조체 포인터 선언

    // 초기 세팅
    SSL_load_error_strings();
    SSLeay_add_ssl_algorithms();
    method = TLS_client_method();
    ctx = SSL_CTX_new(method);
    CHK_NULL(ctx);

    // Context에서 사용할 인증서 설정
    if (SSL_CTX_use_certificate_file(ctx, "./client_auth/Client.crt", SSL_FILETYPE_PEM) <= 0)
    {
        ERR_print_errors_fp(stderr);
        return -1;
    }

    // Context에서 사용할 개인키 설정
    if (SSL_CTX_use_PrivateKey_file(ctx, "./client_auth/privkey-Client.pem", SSL_FILETYPE_PEM) <= 0)
    {
        ERR_print_errors_fp(stderr);
        return -1;
    }

    // 개인키 사용가능성 확인
    if (!SSL_CTX_check_private_key(ctx))
    {
        fprintf(stderr, "개인키 불일치\n");
        return -1;
    }

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0)
    {
        perror("socket");
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(SERVER_PORT);

    // 서버에 연결
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connect");
        return -1;
    }

    // SSL 세션 객체 생성
    ssl = SSL_new(ctx);
    CHK_NULL(ssl);

    // SSL객체에 socket fd를 설정
    SSL_set_fd(ssl, client_socket);
    if (SSL_connect(ssl) == -1) // SSL 연결
    {
        ERR_print_errors_fp(stderr);
        return -1;
    }

    server_cert = SSL_get_peer_certificate(ssl); // 서버 인증서 획득
    if (server_cert == NULL)
    {
        fprintf(stderr, "서버 인증서가 없습니다.\n");
        return -1;
    }
    X509_free(server_cert); // 서버 인증서 해제

    printf("Connected to server\n");

    // 클라우드 함수 실행
    client_cloud_function(client_socket, ssl);

    SSL_free(ssl);
    close(client_socket);
    SSL_CTX_free(ctx);
    return 0;
}

int client_cloud_function(int client_socket, SSL *ssl)
{
    Client client;
    Message msg;
    char buffer[1024];
    int n;

    while (1)
    {
        // 터미널 clear
        if (system("clear") == -1)
        {
            perror("system");
            return -1;
        }

        printf("Enter command: ");
        if (scanf("%s", client.command) == EOF)
        {
            perror("scanf");
            exit(1);
        }
        if (strcmp(client.command, "exit") == 0)
        {
            if (SSL_write(ssl, &client, sizeof(client)) <= 0)
            {
                perror("SSL_write");
                return -1;
            }
            break;
        }
        else if (strcmp(client.command, "upload") == 0 || strcmp(client.command, "UPLOAD") == 0) // 업로드 함수 호출
        {
            user_interface(&client);

            if (exist_file(client.filename) == -1)
            {
                printf("파일이 존재하지 않습니다.\n");
                printf("Enter 키를 누르세요\n");
                get_char();
                continue;
            }
            SSL_write(ssl, &client, sizeof(client));
            SSL_read(ssl, &msg, sizeof(msg));
            if (strncmp(msg.message, "correct", 7) == 0)
            {
                upload(client_socket, client, ssl);
                printf("업로드 완료\n");
            }
            else
            {
                printf("비밀번호가 틀렸습니다. 다시 입력해주세요.\n");
            }
            printf("Enter 키를 누르세요\n");
            get_char();
        }
        else if (strcmp(client.command, "download") == 0 || strcmp(client.command, "DOWNLOAD") == 0) // 다운로드 함수 호출
        {
            user_interface(&client);
            SSL_write(ssl, &client, sizeof(client));
            SSL_read(ssl, &msg, sizeof(msg));
            if (strncmp(msg.message, "correct", 7) == 0)
            {
                download(client_socket, client, ssl);
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
        else if (strcmp(client.command, "list") == 0 || strcmp(client.command, "LIST") == 0) // 파일 목록 출력 함수 호출
        {
            user_interface(&client);
            SSL_write(ssl, &client, sizeof(client));
            SSL_read(ssl, &msg, sizeof(msg));
            if (strncmp(msg.message, "correct", 7) == 0)
            {
                list(client_socket, client, ssl);
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
        else if (strcmp(client.command, "chat") == 0 || strcmp(client.command, "CHAT") == 0) // 채팅 함수 호출
        {
            user_interface(&client);
            SSL_write(ssl, &client, sizeof(client));
            SSL_read(ssl, &msg, sizeof(msg));
            if (strncmp(msg.message, "correct", 7) == 0)
            {
                printf("채팅을 시작합니다.\n");
                chat(ssl);
                printf("채팅을 종료합니다.\n");
            }
            else if (strncmp(msg.message, "incorrect", 9) == 0)
            {
                printf("비밀번호가 틀렸습니다. 다시 입력해주세요.\n");
            }
            else
            {
                printf("디렉토리가 존재하지 않습니다.\n");
            }
            printf("Enter 키를 누르세요");
            get_char();
        }
        else
        {
            printf("잘못된 명령어입니다.\n");
            printf("Enter 키를 누르세요\n");
            get_char();
        }
        memset(&client, 0, sizeof(client));
    }
}

int upload(int client_socket, Client client, SSL *ssl)
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
        SSL_write(ssl, buffer, n);
    }

    printf("%s 업로드 완료\n", client.filename);

    fclose(fp);

    return 0;
}

int download(int client_socket, Client client, SSL *ssl)
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
        if ((received = SSL_read(ssl, buffer, 1024)) < 0)
        {
            perror("SSL_read");
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

int list(int client_socket, Client client, SSL *ssl)
{

    char buffer[1024];
    int received = 0;

    // 서버로부터 파일 리스트를 받아와서 화면에 출력
    while (1)
    {
        if ((received = SSL_read(ssl, buffer, 1024)) <= 0)
        {
            perror("SSL_read");
            return -1;
        }
        if (strncmp(buffer, "EOF", 3) == 0) // 파일 리스트의 끝을 알리는 EOF 문자열을 받으면 종료
        {
            break;
        }
        fwrite(buffer, 1, received, stdout);
    }

    return 0;
}

void chat(SSL *ssl)
{
    pthread_t snd_thread, rcv_thread;
    void *thread_return;

    pthread_create(&snd_thread, NULL, send_msg, (void *)ssl); // 송신 쓰레드 생성
    pthread_create(&rcv_thread, NULL, recv_msg, (void *)ssl); // 수신 쓰레드 생성
    pthread_join(snd_thread, &thread_return);                 // 송신 쓰레드 종료 대기
    pthread_join(rcv_thread, &thread_return);                 // 수신 쓰레드 종료 대기
}
