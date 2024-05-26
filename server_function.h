/*
 * 클라우드 서버의 기타 기능을 정의한 헤더 파일
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <sys/file.h>
#include <sys/sem.h>

void join_sem_lock(int join_sem_id);
void join_sem_unlock(int join_sem_id);
void create_directory_sem_lock(int create_directory_sem_id);
void create_directory_sem_unlock(int create_directory_sem_id);
int join(int client_sock);
int login(int client_sock);
int create_group_directory(const char *group, const char *password);
int create_id_directory(const char *group, const char *id);
int check_file_name(const char *group, const char *id, char *file_name);
int check_password(const char *group, const char *password);

typedef struct
{
    char id[20];       // 사용자 ID
    char group[20];    // 사용자 그룹
    char password[20]; // 사용자 비밀번호
    char name[20];     // 사용자 이름
} Client;              // 사용자 정보 구조체

typedef struct // 구조체 선언
{
    char group[20];
    char password[20];
    char id[20];
    char file_name[256];
} Input;

int join_sem_id;             // 세마포어 ID
int create_directory_sem_id; // 디렉토리 생성 세마포어 ID

void join_sem_lock(int join_sem_id) // 세마포어 잠금 함수
{
    struct sembuf sb = {0, -1, SEM_UNDO}; // 세마포어 설정
    semop(join_sem_id, &sb, 1);           // 세마포어 잠금
}

void join_sem_unlock(int sem_id) // 세마포어 잠금 해제 함수
{
    struct sembuf sb = {0, 1, SEM_UNDO}; // 세마포어 설정
    semop(sem_id, &sb, 1);               // 세마포어 잠금 해제
}

void create_directory_sem_lock(int create_directory_sem_id)
{
    struct sembuf sb = {0, -1, SEM_UNDO};
    semop(create_directory_sem_id, &sb, 1);
}

void create_directory_sem_unlock(int create_directory_sem_id)
{
    struct sembuf sb = {0, 1, SEM_UNDO};
    semop(create_directory_sem_id, &sb, 1);
}

int join(int client_sock)
{
    Client client;
    char buf[1024];
    char id[20];
    char password[20];

    if (recv(client_sock, &client, sizeof(client), 0) == -1) // 클라이언트로부터 사용자 정보 수신
    {
        perror("join recv() error");
        return -1;
    }

    join_sem_lock(join_sem_id);        // 세마포어 잠금
    FILE *fp = fopen("user.txt", "a"); // 사용자 정보 파일 열기, 추가 모드
    if (fp == NULL)                    // 파일 열기 실패 시
    {
        perror("user.txt fopen() error");
        return -1;
    }

    fprintf(fp, "%s %s %s %s\n", client.id, client.password, client.name, client.group); // 사용자 정보 파일에 사용자 정보 추가
    fclose(fp);                                                                          // 사용자 정보 파일 닫기
    join_sem_unlock(join_sem_id);                                                        // 세마포어 잠금 해제

    strcpy(buf, "join success");                      // 회원가입 성공 메시지 저장
    if (send(client_sock, buf, sizeof(buf), 0) == -1) // 클라이언트에게 회원가입 성공 메시지 전송
    {
        perror("send() error");
        return -1;
    }
    return 0;
}

int login(int client_sock)
{
    Client client;
    char buf[1024];
    char id[20];
    char password[20];
    char group[20];

    if (recv(client_sock, &client, sizeof(client), 0) == -1) // 클라이언트로부터 사용자 정보 수신
    {
        perror("login recv() error");
        return -1;
    }

    FILE *fp = fopen("user.txt", "r"); // 사용자 정보 파일 열기, 읽기 모드
    if (fp == NULL)                    // 파일 열기 실패 시
    {
        perror("user.txt fopen() error");
        return -1;
    }

    while (fscanf(fp, "%s %s %s %s\n", id, password, client.name, group) != EOF) // 사용자 정보 파일에서 사용자 정보 읽기
    {
        if (strcmp(client.id, id) == 0 && strcmp(client.password, password) == 0) // 사용자 정보 일치 시
        {
            strcpy(buf, "login success");                     // 로그인 성공 메시지 저장
            if (send(client_sock, buf, sizeof(buf), 0) == -1) // 클라이언트에게 로그인 성공 메시지 전송
            {
                perror("send() error");
                return -1;
            }
            return 0;
        }
    }
}

// 디렉토리 생성 및 비밀번호 파일 생성 함수
int create_group_directory(const char *group, const char *password)
{
    char password_path[512];

    // client_group 디렉토리 생성
    if (mkdir(group, 0777) == -1 && errno != EEXIST)
    {
        perror("create_directory : mkdir() error");
        return -1;
    }

    // 비밀번호 파일 경로 설정
    sprintf(password_path, "%s/password.txt", group);

    // 비밀번호 파일 생성 및 비밀번호 기록
    FILE *fp = fopen(password_path, "w");
    if (fp == NULL)
    {
        perror("create_directory : password.txt fopen() error");
        return -1;
    }
    fprintf(fp, "%s\n", password);
    fclose(fp);

    // 비밀번호 파일 권한 설정
    if (chmod(password_path, 0600) == -1)
    {
        perror("create_directory : chmod() error");
        return -1;
    }

    return 0;
}

int create_id_directory(const char *group, const char *id)
{
    char id_path[512];

    // client_group/client_id 디렉토리 생성
    sprintf(id_path, "%s/%s", group, id);
    if (mkdir(id_path, 0777) == -1 && errno != EEXIST)
    {
        perror("create_id_directory : mkdir() error");
        return -1;
    }

    return 0;
}

// 같은 이름의 파일이 존재하는 경우 파일 이름 뒤에 _1, _2, ...를 붙임
int check_file_name(const char *group, const char *id, char *file_name)
{
    char temp_file_name[256];
    int i = 1;
    sprintf(temp_file_name, "%s/%s/%s", group, id, file_name);
    while (access(temp_file_name, F_OK) != -1)
    {
        sprintf(temp_file_name, "%s/%s/%s_%d", group, id, file_name, i);
        i++;
    }
    strcpy(file_name, temp_file_name);

    return 0;
}

// 디렉토리 비밀번호 확인하기
int check_password(const char *group, const char *password)
{
    char password_path[512];
    char correct_password[20];

    // 비밀번호 파일 경로 설정
    sprintf(password_path, "%s/password.txt", group);

    // 비밀번호 파일 열기
    FILE *fp = fopen(password_path, "r");
    if (fp == NULL)
    {
        perror("check_password : password.txt fopen() error");
        return -1;
    }

    // 비밀번호 읽기
    if (fscanf(fp, "%s\n", correct_password) == EOF)
    {
        perror("check_password : password.txt fscanf() error");
        return -1;
    }
    fclose(fp);

    // 비밀번호 비교
    if (strcmp(password, correct_password) != 0)
    {
        return -1;
    }

    return 0;
}
