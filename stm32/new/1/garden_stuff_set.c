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
char uptemp[20];
char downtemp[20];
char uphume[20];
char downhume[20];
char uplux[20];
char downlux[20];
char buf[6][20];
char temp[20];
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
// 定义写入数据的函数
void write_data_to_file(FILE *file, char *str)
{
    fprintf(file, "%s\n", str); // 将每个字符串写入文件，并换行
}
// 主函数
int cgiMain(int argc, const char *argv[])
{
    int i = 0;
    // 打开文件用于写入（"w"模式表示写入，如果文件不存在则创建，存在则清空）
    FILE *file_limit = fopen("dev_limit.txt", "w+");
    if (file_limit == NULL)
    {
        fprintf(stderr, "无法打开文件\n");
        return 1;
    }
    FILE *file_script_js = fopen("../greenhouse/script.js", "w");
    if (file_script_js == NULL)
    {
        fprintf(stderr, "无法打开文件\n");
        return 1;
    }

    // 注册定时器信号处理函数
    signal(SIGALRM, handle_alarm);
    cgiFormString("temp_up", uptemp, sizeof(uptemp));
    cgiFormString("temp_low", downtemp, sizeof(downtemp));
    cgiFormString("hum_up", uphume, sizeof(uphume));
    cgiFormString("hum_low", downhume, sizeof(downhume));
    cgiFormString("illu_up", uplux, sizeof(uplux));
    cgiFormString("illu_low", downlux, sizeof(downlux));
    printf("Content-type: text/html;charset=\"UTF-8\"\n\n");

    msg.limitdata.uptemp = charArrayToFloat(uptemp);
    msg.limitdata.downtemp = charArrayToFloat(downtemp);
    msg.limitdata.uphume = charArrayToInt(uphume);
    msg.limitdata.downhume = charArrayToInt(downhume);
    msg.limitdata.uplux = charArrayToInt(uplux);
    msg.limitdata.downlux = charArrayToInt(downlux);

    msgid_A = message_init("/home/linux", 'A');
    msgid_B = message_init("/home/linux", 'B');

    extract_name(cgiCookie, idbuf, sizeof(idbuf));
    msgsum = calculate_msgsum(idbuf);
    msg.msgtype = msgsum;
    msg.commd = 2;
    msgsnd(msgid_A, &msg, sizeof(msg_t) - sizeof(long), 0);
    memset(&msg, 0, sizeof(msg_t));
    alarm(5);
    /*等待环境数据结果*/
    msgrcv(msgid_B, &msg, sizeof(msg_t) - sizeof(long), msgsum, 0);
    alarm(0);

    write_data_to_file(file_limit, uptemp);
    write_data_to_file(file_limit, downtemp);
    write_data_to_file(file_limit, uphume);
    write_data_to_file(file_limit, downhume);
    write_data_to_file(file_limit, uplux);
    write_data_to_file(file_limit, downlux);

    // printf("<!DOCTYPE html>");
    // printf("<html>");
    // printf("<body>");
    // printf("<center>");
    // printf("<font color=\"#FFD700\" size=\"+2\">环境设置</font>");
    // printf("<form action=\"/cgi-bin/garden_stuff_set.cgi\" method=\"get\">");
    // printf("<br>");
    // printf("温度上限:");
    // printf("<input type=\"text\" name=\"temp_up\" value=\"%s\" size=10/><br>",uptemp);
    // printf("温度下限:");
    // printf("<input type=\"text\" name=\"temp_low\" value=\"%s\" size=10/><br><br>",downtemp);
    // printf("湿度上限:");
    // printf("<input type=\"text\" name=\"hum_up\" value="" size=10/><br>");
    // printf("湿度下限:");
    // printf("<input type=\"text\" name=\"hum_low\" value="" size=10/><br><br>");
    // printf("光强上限:");
    // printf("<input type=\"text\" name=\"illu_up\" value="" size=10/><br>");
    // printf("光强下限:");
    // printf("<input type=\"text\" name=\"illu_low\" value="" size=10/><br><br>");
    // printf("<br>");
    // //printf("<iframe src=\"/cgi-bin/garden_stuff_set.cgi\" height=\"310\" width=\"300\"></iframe>");
    // printf("<input type=\"submit\" name=\"set_button\" value=\"提交\" />");
    // printf("<input type=\"reset\" name=\"reset\" value=\"重置\"/>");
    // printf("</form> ");
    // printf("</center>");
    // printf("<script src=\"/www/cgi-bin/script.js\"></script> ");
    // printf("</body>");
    // printf("</html> ");

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
    strcpy(downtemp, buf[1]);
    len = strlen(downtemp);
    if (len > 0 && downtemp[len - 1] == '\n')
    {
        downtemp[len - 1] = '\0';
    }
    strcpy(uphume, buf[2]);
    len = strlen(uphume);
    if (len > 0 && uphume[len - 1] == '\n')
    {
        uphume[len - 1] = '\0';
    }
    strcpy(downhume, buf[3]);
    len = strlen(downhume);
    if (len > 0 && downhume[len - 1] == '\n')
    {
        downhume[len - 1] = '\0';
    }
    strcpy(uplux, buf[4]);
    len = strlen(uplux);
    if (len > 0 && uplux[len - 1] == '\n')
    {
        uplux[len - 1] = '\0';
    }
    strcpy(downlux, buf[5]);
    len = strlen(downlux);
    if (len > 0 && downlux[len - 1] == '\n')
    {
        downlux[len - 1] = '\0';
    }

    // 写入JavaScript代码
    fprintf(file_script_js, "const knownData = {\n");
    fprintf(file_script_js, "    temp_up:\"%s\",\n", uptemp);
    fprintf(file_script_js, "    temp_low:\"%s\",\n", downtemp);
    fprintf(file_script_js, "    hum_up:\"%s\",\n", uphume);
    fprintf(file_script_js, "    hum_low:\"%s\",\n", downhume);
    fprintf(file_script_js, "    illu_up:\"%s\",\n", uplux);
    fprintf(file_script_js, "    illu_low:\"%s\",\n", downlux);
    fprintf(file_script_js, "};\n");
    fprintf(file_script_js, "\n");
    fprintf(file_script_js, "const box = document.getElementById('box3');\n");
    fprintf(file_script_js, "const inputsInBox = box.querySelectorAll('input');\n");
    fprintf(file_script_js, "\n");
    fprintf(file_script_js, "inputsInBox.forEach(input => {\n");
    fprintf(file_script_js, "    const name = input.name;\n");
    fprintf(file_script_js, "    if (knownData.hasOwnProperty(name)) {\n");
    fprintf(file_script_js, "        input.value = knownData[name];\n");
    fprintf(file_script_js, "    }\n");
    fprintf(file_script_js, "});\n");
    printf("Content-type: text/html;charset=\"UTF-8\"\n\n"); // 固定格式 必须要加
    printf("<script>window.location.href = '/greenhouse/garden_stuff.html';</script>");
    //printf("<script>window.location.href = '/greenhouse/text.html';</script>");
    // 关闭文件
    fclose(file_limit);
    fclose(file_script_js);
    return 0;
}