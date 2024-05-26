/*
 * 클라우드 서버 기능을 하는 함수들을 정의한 헤더 파일
 */

#include <server_function.h>

int join_or_login(int client_sock);
int cloud_service(int client_sock);
int upload(int client_sock);
int download(int client_sock);
int list(int client_sock);

int join_or_login(int client_sock)
{
    char buf[1024];
    while (1)
    {
        if (recv(client_sock, buf, sizeof(buf), 0) == -1) // 클라이언트로부터 요청 수신
        {
            perror("recv() error");
            return -1;
        }
        if (strcmp(buf, "join") == 0) // 회원가입 요청 시
        {
            if (join(client_sock) == -1) // 회원가입 처리
            {
                continue;
            }
        }
        else if (strcmp(buf, "login") == 0) // 로그인 요청 시
        {
            if (login(client_sock) == -1) // 로그인 처리
            {
                continue;
            }
            else // 로그인 성공 시
            {
                return 0;
            }
        }
        else if (strcmp(buf, "exit") == 0) // 종료 요청 시
        {
            return -1;
        }
    }
}

int cloud_service(int client_sock)
{
    char buf[1024];
    while (1)
    {
        if (recv(client_sock, buf, sizeof(buf), 0) == -1) // 클라이언트로부터 요청 수신
        {
            perror("recv() error");
            return -1;
        }
        if (strcmp(buf, "upload") == 0) // 업로드 요청 시
        {
            if (upload(client_sock) == -1) // 업로드 처리
            {
                continue;
            }
        }
        else if (strcmp(buf, "download") == 0) // 다운로드 요청 시
        {
            if (download(client_sock) == -1) // 다운로드 처리
            {
                continue;
            }
        }
        else if (strcmp(buf, "list") == 0) // 파일 목록 요청 시
        {
            if (list(client_sock) == -1) // 파일 목록 처리
            {
                continue;
            }
        }
        else if (strcmp(buf, "exit") == 0) // 종료 요청 시
        {
            return 0;
        }
    }
}

// client_group/client_id/file_name 경로로 파일 업로드 하기, 동기화 기능 구현
int upload(int client_sock)
{
    char file_path[512];       // 파일 경로
    char buf[1024];            // 파일 데이터 버퍼
    int bytes_received;        // 수신한 바이트 수
    char password_path[512];   // 비밀번호 파일 경로
    char correct_password[20]; // 올바른 비밀번호
    Input input;

    if (recv(client_sock, &input, sizeof(input), 0) == -1)
    {
        perror("upload : input recv() error");
        return -1;
    }

    // 동기화 기능 구현
    create_directory_sem_lock(create_directory_sem_id); // 세마포어 잠금
    if (access(input.group, F_OK) == -1)                // client_group 디렉토리가 존재하지 않을 경우
    {
        if (create_group_directory(input.group, input.password) == -1)
        {
            return -1;
        }
    }

    if (check_password(input.group, input.password) == -1)
    {
        strcpy(buf, "password incorrect");
        if (send(client_sock, buf, sizeof(buf), 0) == -1)
        {
            perror("upload : password incorrect send() error");
            return -1;
        }
        return 0;
    }

    if (access(file_path, F_OK) == -1) // client_group/client_id 디렉토리가 존재하지 않을 경우
    {
        if (create_id_directory(input.group, input.id) == -1)
        {
            return -1;
        }
    }

    check_file_name(input.group, input.id, input.file_name);
    create_directory_sem_unlock(create_directory_sem_id); // 세마포어 잠금 해제

    FILE *fp = fopen(file_path, "w"); // 파일 열기, 쓰기 모드
    if (fp == NULL)                   // 파일 열기 실패 시
    {
        perror("upload : file fopen() error");
        return -1;
    }

    while (1)
    {
        bytes_received = recv(client_sock, buf, sizeof(buf), 0); // 클라이언트로부터 파일 데이터 수신
        if (bytes_received == -1)                                // 파일 데이터 수신 실패 시
        {
            perror("upload : file recv() error");
            fclose(fp); // 파일 닫기
            return -1;
        }
        if (bytes_received == 0) // 파일 데이터 수신 완료 시
        {
            break;
        }

        fwrite(buf, 1, bytes_received, fp); // 파일에 데이터 저장
    }

    fclose(fp); // 파일 닫기
}

// client_group/client_id/file_name 경로에서 파일 다운로드 하기
int download(int client_sock)
{
    char file_path[512];
    char buf[1024];
    int bytes_read;
    char password_path[512];
    char correct_password[20];
    Input input;

    if (recv(client_sock, &input, sizeof(input), 0) == -1)
    {
        perror("download : input recv() error");
        return -1;
    }

    if (check_password(input.group, input.password) == -1)
    {
        strcpy(buf, "password incorrect");
        if (send(client_sock, buf, sizeof(buf), 0) == -1)
        {
            perror("download : password incorrect send() error");
            return -1;
        }
        return 0;
    }

    sprintf(file_path, "%s/%s/%s", input.group, input.id, input.file_name); // 전체 경로 생성

    FILE *fp = fopen(file_path, "r"); // 파일 열기, 읽기 모드
    if (fp == NULL)                   // 파일 열기 실패 시
    {
        perror("download : file fopen() error");
        return -1;
    }

    while (1)
    {
        bytes_read = fread(buf, 1, sizeof(buf), fp); // 파일에서 데이터 읽기
        if (bytes_read == 0)                         // 파일 읽기 완료 시
        {
            break;
        }
        if (send(client_sock, buf, bytes_read, 0) == -1) // 클라이언트에게 파일 데이터 전송
        {
            perror("download : file send() error");
            fclose(fp); // 파일 닫기
            return -1;
        }
    }

    fclose(fp); // 파일 닫기
}

int list(int client_sock)
{
    char buf[1024];
    char file_path[512];
    char group[20];
    char id[20];

    if (recv(client_sock, group, sizeof(group), 0) == -1) // 클라이언트로부터 그룹 정보 수신
    {
        perror("list : group recv() error");
        return -1;
    }
    if (recv(client_sock, id, sizeof(id), 0) == -1) // 클라이언트로부터 아이디 정보 수신
    {
        perror("list : id recv() error");
        return -1;
    }

    sprintf(file_path, "%s/%s", group, id); // 전체 경로 생성

    DIR *dir = opendir(file_path); // 디렉토리 열기
    if (dir == NULL)               // 디렉토리 열기 실패 시
    {
        perror("list : opendir() error");
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) // 디렉토리 내 파일 목록 출력
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }
        strcpy(buf, entry->d_name);
        if (send(client_sock, buf, sizeof(buf), 0) == -1) // 클라이언트에게 파일 목록 전송
        {
            perror("list : send() error");
            return -1;
        }
    }

    closedir(dir); // 디렉토리 닫기
}