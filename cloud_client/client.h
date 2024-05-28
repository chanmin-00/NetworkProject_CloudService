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
    char message[100];
} Message;

int exist_file(char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
    {
        return -1;
    }
    fclose(fp);
    return 0;
}

void get_char(void)
{
    getchar();
    getchar();
}

void user_interface(Client *client)
{
    if (strcmp(client->command, "upload") == 0)
    {
        printf("업로드할 디렉토리 위치를 입력해주세요: ");
        scanf("%s", client->dir);
    }
    else if (strcmp(client->command, "download") == 0)
    {
        printf("다운로드할 디렉토리 위치를 입력해주세요: ");
        scanf("%s", client->dir);
    }
    printf("디렉토리 비밀번호를 입력해주세요: ");
    scanf("%s", client->password);
    printf("파일명을 입력해주세요: ");
    scanf("%s", client->filename);
}
