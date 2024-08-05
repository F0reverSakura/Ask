#include "project.h"
msg_t msg;
long msgsum;
int msgid_B, msgid_A; // 全局变量，用于消息队列ID
char idbuf[20] = {0};

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
    if (-1 == msgrcv(msgid_A, &msg, sizeof(msg_t) - sizeof(long), msgsum, IPC_NOWAIT))
    {
        exit(0);
    }
    
    // 打开网页告诉用户网不好（服务器未响应）
    //  FILE *fp = fopen("./neterror.txt", "r");
    //  if (NULL == fp)
    //  {
    //      exit(0);
    //  }

    // char buf[512] = {0};
    // printf("Content-type: text/html;charset=\"UTF-8\"\n\n"); // 固定格式 必须要加
    // while (fgets(buf, sizeof(buf), fp))
    // {
    //     printf("%s", buf);
    //     memset(buf, 0, 512);
    // }
    // fclose(fp);
    printf("<!DOCTYPE html>");
    printf("<html>");
    printf("<body>");
    printf("<center>");
    printf("刷新数据未响应\n");
    printf("</center>");
    printf("</body>");
    printf("</html>");
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
    int i = 0;
    // 注册定时器信号处理函数
    signal(SIGALRM, handle_alarm);

    printf("Content-type: text/html;charset=\"UTF-8\"\n\n");

    msgid_A = message_init("/home/linux", 'A');
    msgid_B = message_init("/home/linux", 'B');

    extract_name(cgiCookie, idbuf, sizeof(idbuf));
    msgsum = calculate_msgsum(idbuf);
    msg.msgtype = msgsum;
    msg.commd=GETENV;
    msgsnd(msgid_A,&msg,sizeof(msg_t)-sizeof(long),0);
    memset(&msg,0,sizeof(msg_t));
    alarm(5);
    /*等待环境数据结果*/
    msgrcv(msgid_B,&msg,sizeof(msg_t)-sizeof(long),msgsum,0);
    alarm(0);
    printf("<!DOCTYPE html>");
    printf("<html>");
    printf("<body>");
    printf("<center>");
    printf("<table border = \"1\">");
    printf("<tr >");
    printf("<td bgcolor=\"yellow\" width=\"80\" height=\"20\">用户ID</td>");
    printf("<td bgcolor=\"yellow\" width=\"80\" height=\"20\">%s</td>", idbuf);
    printf("</tr>");
    printf("</table>");
    printf("<h3>环境数据</h3>");
    printf("<table border = \"1\">");
    printf("<th bgcolor=\"blue\">温度</th>");
    printf("<th bgcolor=\"blue\">湿度</th>");
    printf("<th bgcolor=\"blue\">光强</th>");
    printf("<tr >");
    printf("<td bgcolor=\"red\" width=\"100\" height=\"20\">up %.2f</td>", msg.limitdata.uptemp);
    printf("<td bgcolor=\"red\" width=\"100\" height=\"20\">up %hhd%%</td>", msg.limitdata.uphume);
    printf("<td bgcolor=\"red\" width=\"100\" height=\"20\">up %hdlux</td>", msg.limitdata.uplux);
    printf("</tr>");
    printf("<tr >");
    printf("<td bgcolor=\"green\" width=\"100\" height=\"20\">%.2f</td>", msg.envdata.temp);
    printf("<td bgcolor=\"green\" width=\"100\" height=\"20\">%hhd%%</td>", msg.envdata.hume);
    printf("<td bgcolor=\"green\" width=\"100\" height=\"20\">%hdlux</td>", msg.envdata.lux);
    printf("</tr>");
    printf("<tr >");
    printf("<td bgcolor=\"yellow\" width=\"100\" height=\"20\">down %.2f</td>", msg.limitdata.downtemp);
    printf("<td bgcolor=\"yellow\" width=\"100\" height=\"20\">down %hhd%%</td>", msg.limitdata.downhume);
    printf("<td bgcolor=\"yellow\" width=\"100\" height=\"20\">down %hdlux</td>", msg.limitdata.downlux);
    printf("</tr>");
    printf("</table>");
    printf("<h3>设备状态</h3>");
    printf("<table>");
    printf("<th bgcolor=\"blue\">照明</th>");
    printf("<th bgcolor=\"blue\">温控</th>");
    printf("<th bgcolor=\"blue\">加湿器</th>");
    printf("<tr >");
    if (msg.envdata.devstuat & (0x01 << 0))
    {
        printf("<td bgcolor=\"red\" width=\"100\" height=\"20\">开</td>");
    }
    else
    {
        printf("<td bgcolor=\"red\" width=\"100\" height=\"20\">关</td>");
    }
    switch ((msg.envdata.devstuat >> 1) & 0x03)
    {
    case 0x01:
        printf("<td bgcolor=\"red\" width=\"100\" height=\"20\">1 档</td>");
        break;
    case 0x02:
        printf("<td bgcolor=\"red\" width=\"100\" height=\"20\">2 档</td>");
        break;
    case 0x03:
        printf("<td bgcolor=\"red\" width=\"100\" height=\"20\">3 档</td>");
        break;
    case 0x00:
        printf("<td bgcolor=\"red\" width=\"100\" height=\"20\">关</td>");
        break;
    }
    if (msg.envdata.devstuat & (0x01 << 3))
    {
        printf("<td bgcolor=\"red\" width=\"100\" height=\"20\">开</td>");
    }
    else
    {
        printf("<td bgcolor=\"red\" width=\"100\" height=\"20\">关</td>");
    }
    printf("</tr>");
    printf("</table>");
    printf("<br>");
    printf("<form action=\"/cgi-bin/garden_stuff_view.cgi\" method=\"POST\">");
    printf("<input type=\"submit\" name=\"button\" value=\"点击刷新数据\">");
    printf("</form>");
    printf("</center>");
    printf("</body>");
    printf("</html>");
    return 0;
}