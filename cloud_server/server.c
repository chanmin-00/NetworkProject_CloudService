#include "server.h"
#include <pthread.h>

#define SERVER_PORT 8080
sem_t semaphore; // 디렉토리 생성 세마포어

int main(int argc, char *argv[])
{
    int server_socket;
    struct sockaddr_in server_addr;
    pthread_t tid;

    // 디렉토리 생성 세마포어 초기화
    if (sem_init(&semaphore, 0, 1) == -1)
    {
        perror("sem_init");
        return -1;
    }

    pthread_mutex_init(&mutx, NULL); // 뮤텍스 변수 초기화

    /*
     * 서버 소켓 생성
     */
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

    /*
     * 서버 소켓에 주소 할당
     */
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

    /*
     *클라이언트 요청을 받아들이고 스레드를 생성하여 처리
     */
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

        // 클라이언트 소켓을 인자로 하는 새로운 스레드를 생성
        if (pthread_create(&tid, NULL, cloud_function_thread, (void *)&client_socket) != 0)
        {
            perror("pthread_create");
            close(client_socket);
        }
        // 스레드를 분리하여 자원 회수
        pthread_detach(tid);
    }

    close(server_socket);
    return 0;
}

void *cloud_function_thread(void *arg)
{
    int client_socket = *((int *)arg);
    Client client;

    while (1)
    {
        if (recv(client_socket, &client, sizeof(client), 0) < 0)
        {
            perror("recv");
            close(client_socket);
            pthread_exit(NULL);
        }

        if (!strcmp(client.command, "UPLOAD") || !strcmp(client.command, "upload"))
        {
            upload(client_socket, client); // 파일 업로드 함수 호출
        }
        else if (!strcmp(client.command, "DOWNLOAD") || !strcmp(client.command, "download"))
        {
            download(client_socket, client); // 파일 다운로드 함수 호출
        }
        else if (!strcmp(client.command, "LIST") || !strcmp(client.command, "list"))
        {
            list(client_socket, client); // 파일 목록 출력 함수 호출
        }
        else if (!strcmp(client.command, "chat") || !strcmp(client.command, "CHAT"))
        {
            chat(client_socket, client); // 채팅 함수 호출
        }
        else if (!strcmp(client.command, "exit"))
        {
            printf("서비스가 종료되었습니다.\n");
            close(client_socket);
            pthread_exit(NULL);
        }
        memset(&client, 0, sizeof(client));
    }
    return NULL;
}

int upload(int client_socket, Client client)
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
    if (check_password(client) == -1)
    {
        sem_post(&semaphore);
        strncpy(msg.message, "incorrect", sizeof("incorrect"));
        send(client_socket, &msg, sizeof(msg), 0);
        return -1;
    }
    else
    {
        sem_post(&semaphore);
        strncpy(msg.message, "correct", sizeof("correct"));
        send(client_socket, &msg, sizeof(msg), 0);
    }

    strcat(client.dir, "/");
    strcat(client.dir, client.filename); // 파일명을 디렉토리 경로에 추가

    sem_wait(&semaphore); // 파일 생성 세마포어 락
    // 원래 파일 이름을 저장
    char base_name[PATH_MAX];
    strcpy(base_name, client.dir);

    while (access(client.dir, F_OK) == 0) // 파일이 이미 존재하면
    {
        // base_filename에서 확장자를 찾아 분리
        char *dot = strrchr(base_name, '.');
        if (dot)
        {
            // base_filename과 확장자를 분리
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
        if ((received = recv(client_socket, buffer, 1024, 0)) < 0)
        {
            perror("recv");
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
}

int download(int client_socket, Client client)
{
    FILE *fp;
    Message msg;
    char buffer[1024];
    int n;

    if (access(client.dir, F_OK) == -1) // 디렉토리가 존재하지 않으면
    {
        strncpy(msg.message, "not exist", sizeof("not exist"));
        send(client_socket, &msg, sizeof(msg), 0); // 클라이언트에게 not exist 메시지 전송
        return -1;
    }

    if (check_password(client) == -1 || strcmp(client.filename, "password.txt") == 0) // 비밀번호가 일치하지 않으면, 파일명이 password.txt인 경우
    {
        strncpy(msg.message, "incorrect", sizeof("incorrect"));
        send(client_socket, &msg, sizeof(msg), 0);
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
            send(client_socket, &msg, sizeof(msg), 0);
            return -1;
        }
        else
        {
            strncpy(msg.message, "correct", sizeof("correct"));
            send(client_socket, &msg, sizeof(msg), 0);
        }
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

    printf("%s 다운로드 완료\n", client.filename);

    fclose(fp);
}

int list(int client_socket, Client client)
{
    DIR *dir;             // 디렉토리 구조체
    struct dirent *entry; // 디렉토리 엔트리 구조체
    Message msg;
    char buffer[1024];
    int n;

    if (check_list_chat(client_socket, client) == -1)
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
        send(client_socket, buffer, strlen(buffer), 0);
    }

    closedir(dir);
}

void chat(int client_socket, Client client)
{
    if (check_list_chat(client_socket, client) == -1)
    {
        return;
    }
    pthread_t t_id;
    Chat_Setting chat_setting;
    chat_setting.clnt_sock = client_socket;
    strcpy(chat_setting.dir, client.dir);

    pthread_mutex_lock(&mutx); // 뮤텍스 락
    clnt_socks[clnt_cnt] = client_socket;
    chat_clnt[clnt_cnt++] = chat_setting; // 클라이언트 소켓과 채팅 설정 구조체 저장
    pthread_mutex_unlock(&mutx);          // 뮤텍스 언락

    pthread_create(&t_id, NULL, chat_client, (void *)&chat_setting); // 채팅 클라이언트 스레드 생성
    pthread_join(t_id, NULL);                                        // 채팅 클라이언트 스레드 종료 대기, 자원 회수
}
