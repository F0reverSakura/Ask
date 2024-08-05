#include "project.h"
msg_t msg;
long msgsum;
int msgrid, msgwid;  // 全局变量，用于消息队列ID
char name[20] = {0}; // 全局变量，用于存储用户名
char password[20] = {0};
// 数据库初始化
sqlite3 *sqlite3_init()
{
    sqlite3 *my_db = NULL;
    int ret = 0;
    // 打开数据库文件
    if (SQLITE_OK != (ret = sqlite3_open("Management_System.db", &my_db)))
    {
        printf("sqlite3_open error : [%d] [%s]\n", ret, sqlite3_errmsg(my_db));
        exit(-1);
    }
    return my_db;
}

// 登录数据比对
int search_login(sqlite3 *my_db, char *name, char *password)
{
    int ret, column, row;
    char sqlbuff[256] = {0};
    char **result;
    sprintf(sqlbuff, "SELECT name,password FROM users WHERE name = '%s' AND password = '%s';", name, password);
    if (SQLITE_OK != (ret = sqlite3_get_table(my_db, sqlbuff, &result, &row, &column, NULL)))
    {
        printf("login_sqlite3_exec error : [%d] [%s]\n", ret, sqlite3_errmsg(my_db));
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

// 消息队列初始化
int message_init(const char *pathname, int proj_id)
{
    key_t key;
    int msgid;
    key = ftok(pathname, proj_id);
    if (key == -1)
    {
        perror("ftok");
        exit(1);
    }
    msgid = msgget(key, 0666 | IPC_CREAT);
    if (msgid == -1)
    {
        perror("msgget");
        exit(1);
    }
    return msgid;
}

// 计算ID字符的ASCII值和
long calculate_msgsum(char *id)
{
    long msgsum = 0;
    while (*id)
    {
        msgsum += *id++;
    }
    return msgsum;
}

// 定时器信号处理函数
void handle_alarm(int sig)
{
    /*查找消息队列中自己的数据*/
    while (-1 == msgrcv(msgwid, &msg, sizeof(msg_t) - sizeof(long), 1, IPC_NOWAIT))
    {
        if (msgsum == msg.rcvtype)
        {
            break;
        }
        msgsnd(msgwid, &msg, sizeof(msg_t) - sizeof(long), 0);
        memset(&msg, 0, sizeof(msg_t));
    }
    // 打开网页告诉用户网不好（服务器未响应）
    //  FILE *fp = fopen("./neterror.txt", "r");
    //  if (NULL == fp)
    //  {
    //      exit(0);
    //  }

    // char buf[512] = {0};
    printf("Content-type: text/html;charset=\"UTF-8\"\n\n"); // 固定格式 必须要加
    // while (fgets(buf, sizeof(buf), fp))
    // {
    //     printf("%s", buf);
    //     memset(buf, 0, 512);
    // }
    // fclose(fp);
    printf("服务器未响应\n");
    exit(0);
}
void extract_name(const char *cgiCookie, char *buf, size_t buf_size)
{
    const char *key = "username=";
    const char *start = strstr(cgiCookie, key);
    if (start != NULL)
    {
        start += strlen(key); // Move past "username="
        const char *end = strchr(start, ';');
        if (end == NULL)
        {
            end = start + strlen(start); // If no ';' found, end at the end of string
        }
        size_t len = end - start;
        if (len >= buf_size)
        {
            len = buf_size - 1; // Ensure we do not overflow the buffer
        }
        strncpy(buf, start, len);
        buf[len] = '\0'; // Null-terminate the string
    }
    else
    {
        buf[0] = '\0'; // If "username=" not found, return an empty string
    }
}
// 主函数
int cgiMain(int argc, const char *argv[])
{
    // 注册定时器信号处理函数
    signal(SIGALRM, handle_alarm);

    // printf("Content-type: text/html;charset=\"UTF-8\"\n\n");

    cgiFormString("ID", name, sizeof(name));
    cgiFormString("PASSWORD", password, sizeof(password));

    msgwid = message_init("/home/linux", 'A');
    msgrid = message_init("/home/linux", 'B');

    // 清空msg结构体
    memset(&msg, 0, sizeof(msg));
    strcpy(msg.login.id, name);
    strcpy(msg.login.ps, password);
    msg.rcvtype = calculate_msgsum(name);
    // printf("%lld\n",msg.rcvtype);
    msg.msgtype = 1;     // 消息类型设为1
    msg.login.flags = 0; // 请求标志位
    if (msgsnd(msgwid, &msg, sizeof(msg) - sizeof(long), 0) == -1)
    {
        perror("msgsnd");
        exit(1);
    }

    // 设置定时器，5秒后发送SIGALRM信号
    alarm(5);

    // 清空msg结构体
    memset(&msg, 0, sizeof(msg));
    // 接受的消息类型为msgsum
    if (msgrcv(msgrid, &msg, sizeof(msg) - sizeof(long), msgsum, 0) == -1)
    {
        if (errno == EIDRM)
        {
            printf("消息队列被删除\n");
        }
        else
        {
            perror("msgrcv");
        }
        exit(1);
    }

    // 取消定时器
    alarm(0);

    if (msg.login.flags == 1)
    {
        // 跳转网页
        printf("Set-Cookie:username=%s;path=/;\n", name);
        printf("Content-type: text/html;charset=\"UTF-8\"\n\n"); // 固定格式 必须要加
        printf("<script>window.location.href = '/Iot_select.html';</script>");
        // printf("%s", cgiCookie);
        // 测试cookie
        // char buf[20] = {0};
        // extract_name(cgiCookie, buf, sizeof(buf));
        // printf("%s\n", buf);
        // msgsum = calculate_msgsum(buf);
        // printf("%ld\n", msgsum);
    }
    else
    {
        // 告诉浏览器下位机不在线或账号密码错误
        printf("Content-type: text/html;charset=\"UTF-8\"\n\n");
        printf("下位机不在线或账号密码错误\n");
    }

    return 0;
}