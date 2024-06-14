#include "server.h"
#include <pthread.h>

#define SERVER_PORT 8080
sem_t semaphore; // 디렉토리 생성 세마포어

int main(int argc, char *argv[])
{
    int server_socket;
    struct sockaddr_in server_addr;
    pthread_t tid;

    SSL_CTX *ctx;             // SSL 컨텍스트 구조체
    SSL *ssl;                 // SSL 구조체
    X509 *client_cert;        // 클라이언트 인증서 구조체
    const SSL_METHOD *method; // SSL 메소드 구조체

    // SSL 라이브러리 초기화
    SSL_load_error_strings();
    SSLeay_add_ssl_algorithms();
    method = TLS_server_method();
    ctx = SSL_CTX_new(method); // SSL 컨텍스트 구조체 생성

    if (!ctx)
    {
        ERR_print_errors_fp(stderr);
        return -1;
    }

    // 서버의 인증서 설정, 인증서 파일이 없으면 에러
    if (SSL_CTX_use_certificate_file(ctx, "./server_auth/Server.crt", SSL_FILETYPE_PEM) <= 0)
    {
        ERR_print_errors_fp(stderr);
        return -1;
    }

    // 서버의 개인키 설정, 개인키 파일이 없으면 에러
    if (SSL_CTX_use_PrivateKey_file(ctx, "./server_auth/privkey-Server.pem", SSL_FILETYPE_PEM) <= 0)
    {
        ERR_print_errors_fp(stderr);
        return -1;
    }

    // 개인키 사용 가능성 체크
    if (!SSL_CTX_check_private_key(ctx))
    {
        fprintf(stderr, "개인키 불일치\n");
        return -1;
    }

    // 사용할 CA의 인증서 설정, CA 인증서 파일이 없으면 에러
    if (!SSL_CTX_load_verify_locations(ctx, "../CA/CA.crt", NULL))
    {
        ERR_print_errors_fp(stderr);
        return -1;
    }

    // Client의 인증서를 검증하기 위한 설정, CA는 하나만 있다고 가정-> depth = 1
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE, verify_callback);
    SSL_CTX_set_verify_depth(ctx, 1);

    // 디렉토리 생성 세마포어 초기화
    if (sem_init(&semaphore, 0, 1) == -1)
    {
        perror("sem_init");
        return -1;
    }
    // 뮤텍스 변수 초기화
    pthread_mutex_init(&mutx, NULL);

    // 서버 소켓 생성
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

    // 서버 소켓에 주소 할당
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

    // 클라이언트 요청을 받아들이고 스레드를 생성하여 처리
    while (1)
    {
        int client_socket;
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket < 0)
        {
            perror("accept");
            return -1;
        }

        ssl = SSL_new(ctx); // SSL 구조체 생성
        CHK_NULL(ssl);      // SSL 구조체 생성 실패 시 종료

        SSL_set_fd(ssl, client_socket); // 클라이언트 소켓을 SSL 소켓으로 설정
        if (SSL_accept(ssl) <= 0)
        {
            ERR_print_errors_fp(stderr);
            close(client_socket);
            SSL_free(ssl);
            continue;
        }

        client_cert = SSL_get_peer_certificate(ssl); // 클라이언트 인증서
        if (client_cert == NULL)
        {
            fprintf(stderr, "인증서가 없습니다.\n");
            close(client_socket);
            SSL_free(ssl);
            continue;
        }

        if (SSL_get_verify_result(ssl) == X509_V_OK) // 클라이언트 인증서 검증
        {
            printf("인증서 검증 성공\n");
            X509_free(client_cert);
        }
        else
        {
            printf("인증서 검증 실패\n");
            close(client_socket);
            SSL_free(ssl);
            continue;
        }

        // 클라이언트 소켓을 인자로 하는 새로운 스레드를 생성
        if (pthread_create(&tid, NULL, cloud_function_thread, (void *)ssl) != 0)
        {
            perror("pthread_create");
            close(client_socket);
        }
        // 스레드를 분리하여 기다리지 않음
        pthread_detach(tid);
    }

    close(server_socket);
    return 0;
}

void *cloud_function_thread(void *arg)
{
    SSL *ssl = (SSL *)arg;
    int client_socket = SSL_get_fd(ssl);
    Client client;
    while (1)
    {
        if (SSL_read(ssl, &client, sizeof(client)) < 0)
        {
            perror("SSL_read");
            close(client_socket);
            SSL_free(ssl);
            pthread_exit(NULL);
        }
        if (!strcmp(client.command, "UPLOAD") || !strcmp(client.command, "upload"))
        {
            upload(client_socket, client, ssl); // 파일 업로드 함수 호출
        }
        else if (!strcmp(client.command, "DOWNLOAD") || !strcmp(client.command, "download"))
        {
            download(client_socket, client, ssl); // 파일 다운로드 함수 호출
        }
        else if (!strcmp(client.command, "LIST") || !strcmp(client.command, "list"))
        {
            list(client_socket, client, ssl); // 파일 목록 출력 함수 호출
        }
        else if (!strcmp(client.command, "chat") || !strcmp(client.command, "CHAT"))
        {
            chat(client_socket, client, ssl); // 채팅 함수 호출
        }
        else if (!strcmp(client.command, "exit"))
        {
            printf("서비스가 종료되었습니다.\n");
            close(client_socket);
            SSL_free(ssl);
            pthread_exit(NULL);
        }
        memset(&client, 0, sizeof(client));
    }
    return NULL;
}

int upload(int client_socket, Client client, SSL *ssl)
{
    FILE *file;
    Message msg;
    char buffer[1024];
    int file_size = 0;
    int file_index = 0;
    int received = 0;
    int remain_data = 0;
    char tmp_filename[256]; // 임시 저장

    sem_wait(&semaphore);               // 디렉토리 생성 세마포어 락
    if (access(client.dir, F_OK) == -1) // 디렉토리가 존재하지 않으면 생성
    {
        if (create_directory(client) == -1)
        {
            sem_post(&semaphore);
            return -1;
        }
    }
    if (check_password(client) == -1) // 비밀번호가 일치하지 않으면
    {
        sem_post(&semaphore);
        strncpy(msg.message, "incorrect", sizeof("incorrect"));
        if (SSL_write(ssl, &msg, sizeof(msg)) < 0)
        {
            perror("SSL_write");
        }
        return -1;
    }
    else
    {
        sem_post(&semaphore);
        strncpy(msg.message, "correct", sizeof("correct"));
        SSL_write(ssl, &msg, sizeof(msg));
    }

    strcat(client.dir, "/");
    strcat(client.dir, client.filename); // 파일명을 디렉토리 경로에 추가

    sem_wait(&semaphore); // 파일 생성 세마포어 락
    char base_name[256];
    strcpy(base_name, client.dir);

    while (access(client.dir, F_OK) == 0) // 파일이 이미 존재하면
    {
        char *dot = strrchr(base_name, '.'); // base_filename에서 확장자를 찾아 분리
        if (dot)
        {
            size_t base_len = dot - base_name;
            snprintf(tmp_filename, sizeof(tmp_filename), "%.*s_%d%s", (int)base_len, base_name, file_index, dot);
        }
        else
        {
            snprintf(tmp_filename, sizeof(tmp_filename), "%s_%d", base_name, file_index);
        }
        strcpy(client.dir, tmp_filename);
        file_index++;
    }
    sem_post(&semaphore); // 파일 생성 세마포어 언락

    file = fopen(client.dir, "wb"); // 파일 생성, 쓰기 모드, 바이너리 파일
    if (file == NULL)
    {
        perror("fopen");
        return -1;
    }

    while (1)
    {
        if ((received = SSL_read(ssl, buffer, 1024)) < 0)
        {
            perror("SSL_read");
            return -1;
        }
        file_size += received;
        fwrite(buffer, 1, received, file);
        if (received < 1024)
        {
            break; // 파일 전송 완료
        }
    }
    printf("%s 업로드 완료\n", client.filename);
    fclose(file);
    return 0;
}

int download(int client_socket, Client client, SSL *ssl)
{
    FILE *fp;
    Message msg;
    char buffer[1024];
    int n;

    if (access(client.dir, F_OK) == -1) // 디렉토리가 존재하지 않으면
    {
        strncpy(msg.message, "not exist", sizeof("not exist"));
        SSL_write(ssl, &msg, sizeof(msg));
        return -1;
    }

    if (check_password(client) == -1 || strcmp(client.filename, "password.txt") == 0) // 비밀번호가 일치하지 않으면, 파일명이 password.txt인 경우
    {
        strncpy(msg.message, "incorrect", sizeof("incorrect"));
        SSL_write(ssl, &msg, sizeof(msg));
        return -1;
    }
    else
    {
        strcat(client.dir, "/");
        strcat(client.dir, client.filename);
        fp = fopen(client.dir, "rb"); // 파일 열기, 읽기 모드, 바이너리 파일
        if (fp == NULL)
        {
            strncpy(msg.message, "not exist", sizeof("not exist"));
            SSL_write(ssl, &msg, sizeof(msg));
            return -1;
        }
        else
        {
            strncpy(msg.message, "correct", sizeof("correct"));
            SSL_write(ssl, &msg, sizeof(msg));
        }
    }

    while (1)
    {
        n = fread(buffer, 1, sizeof(buffer), fp);
        if (n <= 0)
        {
            break;
        }
        if (SSL_write(ssl, buffer, n) < 0)
        {
            perror("SSL_write");
            return -1;
        }
    }

    printf("%s 다운로드 완료\n", client.filename);

    fclose(fp);
    return 0;
}

int list(int client_socket, Client client, SSL *ssl)
{
    DIR *dir;             // 디렉토리 구조체
    struct dirent *entry; // 디렉토리 엔트리 구조체
    Message msg;
    char buffer[1024];

    if (check_list_chat(client_socket, client, ssl) == -1)
    {
        return -1;
    }

    dir = opendir(client.dir);
    if (dir == NULL)
    {
        perror("opendir");
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) // 디렉토리 엔트리 읽기
    {
        if (entry->d_name[0] == '.') // 숨김 파일은 출력하지 않음
        {
            continue;
        }
        sprintf(buffer, "%s\n", entry->d_name); // 파일명 출력
        SSL_write(ssl, buffer, strlen(buffer));
    }
    strcpy(buffer, "EOF");
    SSL_write(ssl, buffer, strlen(buffer)); // 파일 목록 출력 완료
    printf("파일 목록 출력 완료\n");

    closedir(dir);
}

void chat(int client_socket, Client client, SSL *ssl)
{
    if (check_list_chat(client_socket, client, ssl) == -1)
    {
        return;
    }
    pthread_t t_id;
    Chat_Setting chat_setting;              // 채팅 설정 구조체
    chat_setting.clnt_sock = client_socket; // 클라이언트 소켓
    chat_setting.ssl = ssl;                 // SSL 구조체
    strcpy(chat_setting.dir, client.dir);   // 채팅 방 디렉토리 경로

    pthread_mutex_lock(&mutx); // 뮤텍스 락
    clnt_socks[clnt_cnt] = client_socket;
    chat_clnt[clnt_cnt++] = chat_setting; // 클라이언트 소켓과 채팅 설정 구조체 저장
    pthread_mutex_unlock(&mutx);          // 뮤텍스 언락

    pthread_create(&t_id, NULL, chat_client, (void *)&chat_setting); // 채팅 클라이언트 스레드 생성
    pthread_join(t_id, NULL);                                        // 채팅 클라이언트 스레드 종료 대기, 자원 회수
}
