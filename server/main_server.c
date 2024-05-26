/*
 * Client와의 연결을 통해 클라이언트의 요청을 처리하는 서버 프로그램
 */

#include "cloud_server.h"

#define PORT 9000 // 서버 포트 번호

int main(int argc, char *argv[])
{
    int server_sock;                // 서버 소켓
    int client_sock;                // 클라이언트 소켓
    struct sockaddr_in server_addr; // 서버 주소 구조체
    struct sockaddr_in client_addr; // 클라이언트 주소 구조체
    int client_addr_size;           // 클라이언트 주소 구조체 크기
    pid_t pid;                      // 자식 프로세스의 pid를 저장할 변수

    join_sem_id = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600); // 세마포어 생성
    if (join_sem_id == -1)                                  // 세마포어 생성 실패 시
    {
        perror("main server : semget() error");
        exit(1);
    }
    semctl(join_sem_id, 0, SETVAL, 1); // 세마포어 초기화

    create_directory_sem_id = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600); // 세마포어 생성
    if (create_directory_sem_id == -1)                                  // 세마포어 생성 실패 시
    {
        perror("main server : semget() error");
        exit(1);
    }
    semctl(create_directory_sem_id, 0, SETVAL, 1); // 세마포어 초기화

    memset(&server_addr, 0, sizeof(server_addr));    // 서버 주소 구조체 초기화
    server_addr.sin_family = AF_INET;                // IPv4 주소 체계 사용
    server_addr.sin_port = htons(PORT);              // 포트 설정
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 서버 주소 설정, INADDR_ANY는 모든 인터페이스에서 접속을 허용

    server_sock = socket(AF_INET, SOCK_STREAM, 0); // TCP 서버 소켓 생성
    if (server_sock == -1)                         // 소켓 생성 실패 시
    {
        perror("main server : socket() error");
        exit(1);
    }

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) // 서버 소켓에 주소 할당
    {
        perror("main server : bind() error");
        exit(1);
    }

    if (listen(server_sock, 100) == -1) // 클라이언트 접속 대기열 설정, 최대 100개의 클라이언트 접속 대기
    {
        perror("main server : listen() error");
        exit(1);
    }

    while (1) // 클라우드 서비스 클라이언트 접속 대기 및 처리 루프
    {
        client_addr_size = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_size); // 클라이언트 접속 대기
        if (client_sock < 0)                                                                   // 클라이언트 접속 실패 시
        {
            if (errno == EINTR) // 시그널에 의한 인터럽트 시
            {
                continue;
            }
            perror("main server : accept() error");
            exit(1);
        }

        pid = fork();  // 자식 프로세스 생성
        if (pid == -1) // fork() 실패 시
        {
            perror("main server : fork() error");
            exit(1);
        }
        if (pid == 0) // 자식 프로세스일 경우
        {
            close(server_sock); // 자식 프로세스에서는 서버 소켓을 사용하지 않으므로 닫음

            if (join_or_login(client_sock) == -1) // 클라이언트 로그인 또는 회원가입 처리
            {
                close(client_sock); // 클라이언트 소켓 닫음
                exit(0);            // 자식 프로세스 종료, 클라이언트 요청 처리 후 종료
            }
            cloud_service(client_sock); // 클라우드 서버 기능 처리
            close(client_sock);         // 클라이언트 소켓 닫음
        }
        else // 부모 프로세스일 경우
        {
            close(client_sock); // 부모 프로세스에서는 클라이언트 소켓을 사용하지 않으므로 닫음
        }
    }

    close(server_sock);                           // 서버 소켓 닫음
    semctl(join_sem_id, 0, IPC_RMID);             // 세마포어 제거
    semctl(create_directory_sem_id, 0, IPC_RMID); // 세마포어 제거
    return 0;
}
