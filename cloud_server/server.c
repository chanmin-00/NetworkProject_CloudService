#include "server.h"
#include <pthread.h>

#define SERVER_PORT 8080
sem_t directory_semaphore;
int upload(int socket, Client client);
int download(int client_socket, Client client);
int list(int client_socket, Client client);
void *cloud_function_thread(void *arg);

int main(int argc, char *argv[])
{
    int server_socket;
    struct sockaddr_in server_addr;
    pthread_t tid;

    // 디렉토리 생성 세마포어 초기화
    if (sem_init(&directory_semaphore, 0, 1) == -1)
    {
        perror("sem_init");
        return -1;
    }

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
            upload(client_socket, client);
        }
        else if (!strcmp(client.command, "DOWNLOAD") || !strcmp(client.command, "download"))
        {
            download(client_socket, client);
        }
        else if (!strcmp(client.command, "LIST") || !strcmp(client.command, "list"))
        {
            list(client_socket, client);
        }
        else
        {
            printf("종료\n");
            close(client_socket);
            pthread_exit(NULL);
        }
    }

    return NULL;
}

int upload(int client_socket, Client client)
{
    FILE *file;
    Message msg;
    char buffer[1024];
    int file_size = 0;
    int received = 0;
    int remain_data = 0;

    sem_wait(&directory_semaphore);
    if (access(client.dir, F_OK) == -1)
    {
        if (create_directory(client) == -1)
        {
            sem_post(&directory_semaphore);
            return -1;
        }
    }
    if (check_password(client) == -1)
    {
        sem_post(&directory_semaphore);
        strncpy(msg.message, "incorrect", sizeof("incorrect"));
        send(client_socket, &msg, sizeof(msg), 0);
        return -1;
    }
    else
    {
        sem_post(&directory_semaphore);
        strncpy(msg.message, "correct", sizeof("correct"));
        send(client_socket, &msg, sizeof(msg), 0);
    }

    strcat(client.dir, "/");
    strcat(client.dir, client.filename);
    file = fopen(client.dir, "wb");
    if (file == NULL)
    {
        perror("fopen");
        return -1;
    }

    flock(fileno(file), LOCK_EX); // 파일 잠금 설정
    while (1)
    {
        if ((received = recv(client_socket, buffer, 1024, 0)) < 0)
        {
            flock(fileno(file), LOCK_UN);
            perror("recv");
            return -1;
        }
        file_size += received;
        fwrite(buffer, 1, received, file);
        if (received < 1024)
        {
            break;
        }
    }
    flock(fileno(file), LOCK_UN); // 파일 잠금 해제

    printf("%d : %s 업로드 완료\n", getpid(), client.filename);

    fclose(file);
}

int download(int client_socket, Client client)
{
    FILE *fp;
    Message msg;
    char buffer[1024];
    int n;

    if (access(client.dir, F_OK) == -1)
    {
        strncpy(msg.message, "not exist", sizeof("not exist"));
        send(client_socket, &msg, sizeof(msg), 0);
        return -1;
    }

    if (check_password(client) == -1)
    {
        strncpy(msg.message, "incorrect", sizeof("incorrect"));
        send(client_socket, &msg, sizeof(msg), 0);
        return -1;
    }
    else
    {
        strcat(client.dir, "/");
        strcat(client.dir, client.filename);
        fp = fopen(client.dir, "rb");
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

    printf("%d : %s 다운로드 완료\n", getpid(), client.filename);

    fclose(fp);
}

int list(int client_socket, Client client)
{
    DIR *dir;
    struct dirent *entry; // 디렉토리 엔트리 구조체
    Message msg;
    char buffer[1024];
    int n;

    if (access(client.dir, F_OK) == -1)
    {
        strncpy(msg.message, "not exist", sizeof("not exist"));
        send(client_socket, &msg, sizeof(msg), 0);
        return -1;
    }

    if (check_password(client) == -1)
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