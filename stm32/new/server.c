#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include <sqlite3.h>
#include "project.h"
#include <sys/time.h>

#define BUFFER_SIZE 1024
#define MAX_CLIENTS 100
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
sqlite3 *my_db;
int msgid_A, msgid_B;
int search_login(sqlite3 *my_db, char *name, char *password)
{
    int ret, column, row;
    char sqlbuff[256] = {0};
    char **result;
    sprintf(sqlbuff, "SELECT name,password FROM users WHERE name = '%s' AND password = '%s';", name, password);
    if (SQLITE_OK != (ret = sqlite3_get_table(my_db, sqlbuff, &result, &row, &column, NULL)))
    {
        printf("sever_sqlite3_exec error : [%d] [%s]\n", ret, sqlite3_errmsg(my_db));
        exit(-1);
    }
    if (row == 0)
    {
        return -1;
    }
    if (strcmp(name, result[column]) || strcmp(password, result[column + 1]))
    {
        return -1;
    }
    return 0;
}
long calculate_msgsum(char *id)
{
    long msgsum = 0;
    while (*id)
    {
        msgsum += *id++;
    }
    return msgsum;
}
typedef struct client
{
    int socket;
    pthread_t tid;
    struct client *next;
    long msgsum;
} client_t;

client_t *client_list = NULL;
pthread_mutex_t client_list_mutex = PTHREAD_MUTEX_INITIALIZER;

void *handle_client(void *client_socket);
int get_port_from_args(int argc, char *argv[]);
int init_server_socket(int port);
void add_client(client_t *new_client);
void remove_client(client_t *client);
void init_client_list();
int initialize_message_queue(const char *path, char proj_id, int *msgid);
void *messages_type_handle(void *arg);
int env_msg(msg_t msg, client_t *client);
// 遍历链表并打印每个节点的值
void traverse(client_t *list)
{
    client_t *current = list->next; // 从头结点的下一个节点开始
    pthread_mutex_lock(&clients_mutex);
    while (current != NULL)
    {
        printf("%d ", current->socket);
        current = current->next;
    }
    pthread_mutex_unlock(&clients_mutex);
    printf("\n");
}

void handle_client_disconnection(client_t *client)
{
    close(client->socket);
    remove_client(client);
    traverse(client_list);
    pthread_exit(NULL);
}
// 功能模块：将字符数组中的浮点数转换为float变量
float charArrayToFloat(const char *charArray)
{
    // 使用标准库函数 atof 将字符数组转换为浮点数
    float result = atof(charArray);
    return result;
}
// 功能模块：将字符数组中的整数转换为int变量
int charArrayToInt(const char *charArray)
{
    // 使用标准库函数 atoi 将字符数组转换为整数
    int result = atoi(charArray);
    return result;
}
int env_msg(msg_t msg, client_t *client)
{
    char buf[6][20];
    char temp[20];
    struct timeval timeout;
    int ret;
    char uptemp[20];
    char downtemp[20];
    char uphume[20];
    char downhume[20];
    char uplux[20];
    char downlux[20];
    if ((ret = send(client->socket, &msg, sizeof(msg_t), 0)) < 0)
    {
        handle_client_disconnection(client);
    }
    else if (ret == 0)
    {
        handle_client_disconnection(client);
    }
    memset(&msg,0,sizeof(msg_t));
    // 设置接收超时时间
    timeout.tv_sec = 2;  // 秒
    timeout.tv_usec = 0; // 微秒
    ret = setsockopt(client->socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    if (ret < 0)
    {
        handle_client_disconnection(client);
    }
    if ((ret = recv(client->socket, &msg, sizeof(msg_t), 0)) < 0)
    {
        handle_client_disconnection(client);
    }
    else if (ret == 0)
    {
        handle_client_disconnection(client);
    }
    printf("我发送的msg.commd=%d,msg.devctl=%d\n", msg.commd,msg.devctl);

    // 打开文件用于写入（"w"模式表示写入，如果文件不存在则创建，存在则清空）
    FILE *file_limit = fopen("/www/cgi-bin/dev_limit.txt", "r");
    if (file_limit == NULL)
    {
        fprintf(stderr, "无法打开文件\n");
        return 1;
    }
    if (fseek(file_limit, 0, SEEK_SET) != 0)
    {
        perror("Failed to seek in file");
        fclose(file_limit);
        return EXIT_FAILURE;
    }
    int j = 0;
    while (j < 6)
    {
        fgets(temp, sizeof(temp), file_limit);
        strcpy(buf[j], temp);
        j++;
    }
    size_t len;
    strcpy(uptemp, buf[0]);
    len = strlen(uptemp);
    if (len > 0 && uptemp[len - 1] == '\n')
    {
        uptemp[len - 1] = '\0';
    }
    msg.limitdata.uptemp=charArrayToFloat(uptemp);
    strcpy(downtemp, buf[1]);
    len = strlen(downtemp);
    if (len > 0 && downtemp[len - 1] == '\n')
    {
        downtemp[len - 1] = '\0';
    }
    msg.limitdata.downtemp=charArrayToFloat(downtemp);
    strcpy(uphume, buf[2]);
    len = strlen(uphume);
    if (len > 0 && uphume[len - 1] == '\n')
    {
        uphume[len - 1] = '\0';
    }
    msg.limitdata.uphume=charArrayToInt(uphume);
    strcpy(downhume, buf[3]);
    len = strlen(downhume);
    if (len > 0 && downhume[len - 1] == '\n')
    {
        downhume[len - 1] = '\0';
    }
    msg.limitdata.downhume=charArrayToInt(downhume);
    strcpy(uplux, buf[4]);
    len = strlen(uplux);
    if (len > 0 && uplux[len - 1] == '\n')
    {
        uplux[len - 1] = '\0';
    }
    msg.limitdata.uplux=charArrayToInt(uplux);
    strcpy(downlux, buf[5]);
    len = strlen(downlux);
    if (len > 0 && downlux[len - 1] == '\n')
    {
        downlux[len - 1] = '\0';
    }
    msg.limitdata.downlux=charArrayToInt(downlux);
    if (msgsnd(msgid_B, &msg, sizeof(msg) - sizeof(long), 0) == -1)
    {
        perror("msgsnd\n");
        handle_client_disconnection(client);
    }
}

void *handle_client(void *new_client)
{
    client_t *client = (client_t *)new_client;
    msg_t msg;

    if ((read(client->socket, &msg, sizeof(msg_t))) > 0)
    {
        if (search_login(my_db, msg.login.id, msg.login.ps) == 0)
        {
            client->msgsum = calculate_msgsum(msg.login.id);

            pthread_mutex_lock(&clients_mutex);
            add_client(client);
            pthread_mutex_unlock(&clients_mutex);

            msg.login.flags = 1;
            if (send(client->socket, &msg, sizeof(msg_t), 0) == -1)
            {
                perror("服务器发送失败1\n");
                handle_client_disconnection(client);
            }
            printf("客户端%d登陆成功\n", client->socket);
            // printf("msgnum=%ld\n", client->msgsum);

            while (1)
            {
                //printf("我进入了循环\n");
                if (msgrcv(msgid_A, &msg, sizeof(msg) - sizeof(long), client->msgsum, 0) == -1)
                {
                    perror("msgsnd\n");
                    exit(1);
                }
                printf("我收到的msg.commd=%d||", msg.commd);
                switch (msg.commd)
                {
                case 1:

                case 2:

                case 3:
                    env_msg(msg, client);
                    break;
                case 255:

                    break;
                default:

                    break;
                }
            }
        }
        else
        {
            // 客户端登录失败
            msg.login.flags = 0;
            if (send(client->socket, &msg, sizeof(msg_t), 0) == -1)
            {
                perror("登录失败\n");
            }
            handle_client_disconnection(client);
        }
    }
    else
    {
        // 服务器发送失败2
        msg.login.flags = 0;
        if (send(client->socket, &msg, sizeof(msg_t), 0) == -1)
        {
            perror("服务器发送失败2\n");
        }
        handle_client_disconnection(client);
    }

    return NULL;
}
// 上位机登录，检测下位机是否在线
void *messages_type_handle(void *arg)
{
    msg_t msg;
    msg_t msg_temp;
    while (1)
    {
        // 接收消息队列中的消息
        if (msgrcv(msgid_A, &msg, sizeof(msg) - sizeof(long), 1, 0) == -1)
        {
            perror("msgrcv\n");
            continue;
        }
        printf("id=%s,ps=%s\n",msg.login.id,msg.login.ps);
        msg_temp.rcvtype = msg.rcvtype;
        // 处理接收到的消息
        unsigned char result = 0;
        // printf("login.msgnum=%lld\n", msg.rcvtype);
        //  判断下位机是否存在,并且置位msg_temp.login.flags标志位
        if (search_login(my_db, msg.login.id, msg.login.ps) == 0)
        {
            pthread_mutex_lock(&clients_mutex);
            client_t *current = client_list->next;
            while (current != NULL)
            {
                if (current->msgsum == msg.rcvtype)
                {
                    // printf("login.result = 1\n");
                    result = 1;
                    break;
                }
                current = current->next;
            }
            pthread_mutex_unlock(&clients_mutex);
        }

        // 发送消息
        msg_temp.login.flags = result;
        msg_temp.msgtype = msg.rcvtype;
        if (msgsnd(msgid_B, &msg_temp, sizeof(msg_temp) - sizeof(long), 0) == -1)
        {
            perror("msgrcv\n");
        }
    }
    return NULL;
}

int get_port_from_args(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    if (port <= 0)
    {
        fprintf(stderr, "Invalid port number.\n");
        exit(EXIT_FAILURE);
    }

    return port;
}
// 初始化网络连接
int init_server_socket(int port)
{
    int server_socket;
    struct sockaddr_in server_addr;

    // 创建服务器套接字
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 配置服务器地址结构
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // 绑定服务器套接字
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("Socket bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // 监听连接
    if (listen(server_socket, MAX_CLIENTS) == -1)
    {
        perror("Socket listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    return server_socket;
}
// 客户端链表初始化
void init_client_list()
{
    client_list = (client_t *)malloc(sizeof(client_t));
    if (client_list == NULL)
    {
        perror("Failed to allocate memory for client list head");
        exit(EXIT_FAILURE);
    }
    client_list->next = NULL;
}
// 头插法插入客户端链表
void add_client(client_t *new_client)
{
    // client_t *new_client = (client_t *)malloc(sizeof(client_t));
    if (new_client == NULL)
    {
        perror("Failed to allocate memory for new client");
        return;
    }

    pthread_mutex_lock(&client_list_mutex);
    new_client->next = client_list->next;
    client_list->next = new_client;
    pthread_mutex_unlock(&client_list_mutex);
}

void remove_client(client_t *client)
{
    pthread_mutex_lock(&clients_mutex);
    client_t *current = client_list->next;
    client_t *previous = NULL;

    while (current != NULL)
    {
        if (current == client)
        {
            if (previous == NULL) // Deleting the head of the list
            {
                client_list->next = current->next;
            }
            else
            {
                previous->next = current->next;
            }
            free(current);
            break;
        }
        previous = current;
        current = current->next;
    }

    pthread_mutex_unlock(&clients_mutex);
}
// 消息队列初始化
int initialize_message_queue(const char *path, char proj_id, int *msgid)
{
    key_t key_send;

    // 生成消息队列的key
    key_send = ftok(path, proj_id);
    if (key_send == -1)
    {
        perror("ftok send");
        return -1;
    }

    // 创建消息队列
    *msgid = msgget(key_send, 0666 | IPC_CREAT);
    if (*msgid == -1)
    {
        perror("msgget send");
        return -1;
    }

    return 0;
}
int main(int argc, char *argv[])
{
    int port = get_port_from_args(argc, argv);
    int server_socket = init_server_socket(port);
    if (sqlite3_open("Management_System.db", &my_db) != SQLITE_OK)
    {
        printf("sqlite3_open error : [%s]\n", sqlite3_errmsg(my_db));
        exit(-1);
    }
    // 初始化消息队列
    if (initialize_message_queue("/home/linux", 'A', &msgid_A) == -1)
    {
        return 1;
    }
    if (initialize_message_queue("/home/linux", 'B', &msgid_B) == -1)
    {
        return 1;
    }

    // 客户端链表初始化
    init_client_list();

    struct sockaddr_in client_addr;
    socklen_t addr_size;
    pthread_t tid;

    printf("Server is listening on port %d\n", port);

    // 创建消息接收线程
    pthread_t recv_tid;
    if (pthread_create(&recv_tid, NULL, messages_type_handle, NULL) != 0)
    {
        perror("Message receive thread creation failed");
        return 1;
    }

    while (1)
    {
        // 接受客户端连接
        addr_size = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size);
        if (client_socket == -1)
        {
            perror("Socket accept failed");
            continue;
        }

        printf("Connected to client: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // 创建一个线程来处理客户端通信
        pthread_t client_tid;
        client_t *new_client = (client_t *)malloc(sizeof(client_t));
        new_client->socket = client_socket;
        new_client->next = NULL;
        new_client->msgsum = 0;

        pthread_create(&client_tid, NULL, handle_client, (void *)new_client);
        pthread_detach(client_tid);
    }

    return 0;
}