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

typedef struct
{
    char command[10];
    char dir[100];
    char password[100];
    char filename[100];
} Client;

typedef struct
{
    char tf[100];
} password_tf;

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