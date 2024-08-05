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

// 定时器信号处理函数
void handle_alarm(int sig)
{
    /*查找消息队列中自己的数据*/
    if (-1 == msgrcv(msgid_A, &msg, sizeof(msg_t) - sizeof(long), msgsum, IPC_NOWAIT))
    {
        exit(0);
    }
    //printf("Content-type: text/html;charset=\"UTF-8\"\n\n"); // 固定格式 必须要加
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

char LED[20];
char FAN[20];
char BEEP[20];

char buf[3][20];
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
// 主函数
int cgiMain(int argc, const char *argv[])
{
    int i = 0;
    // 打开文件用于写入（"w"模式表示写入，如果文件不存在则创建，存在则清空）
    FILE *file_dev = fopen("dev_ctrl.txt", "w+");
    if (file_dev == NULL)
    {
        fprintf(stderr, "无法打开文件\n");
        return 1;
    }
    FILE *file_dev_js = fopen("../greenhouse/dev.js", "w");
    if (file_dev_js == NULL)
    {
        fprintf(stderr, "无法打开文件\n");
        return 1;
    }

    // 注册定时器信号处理函数
    signal(SIGALRM, handle_alarm);
    cgiFormString("LED", LED, sizeof(LED));
    cgiFormString("FAN", FAN, sizeof(FAN));
    cgiFormString("BEEP", BEEP, sizeof(BEEP));
    printf("Content-type: text/html;charset=\"UTF-8\"\n\n");

    msg.devctl = 0;
    // 定义各个状态的值
    unsigned char led_bit;        // 灯状态，bit 0
    unsigned char fan_bits;       // 风扇状态，bits 1-2 (二进制为10)
    unsigned char humidifier_bit; // 加湿器状态，bit 3
    if (strcmp(LED, "OFF") == 0)
    {
        led_bit = 0;
        msg.devctl |= led_bit;
    }
    else
    {
        led_bit = 1;
        msg.devctl |= led_bit;
    }

    if (strcmp(FAN, "0") == 0)
    {
        fan_bits = 0;
        msg.devctl |= (fan_bits << 1);
    }
    else if (strcmp(FAN, "1") == 0)
    {
        fan_bits = 1;
        msg.devctl |= (fan_bits << 1);
    }
    else if (strcmp(FAN, "2") == 0)
    {
        fan_bits = 2;
        msg.devctl |= (fan_bits << 1);
    }
    else
    {
        fan_bits = 3;
        msg.devctl |= (fan_bits << 1);
    }

    if (strcmp(BEEP, "OFF") == 0)
    {
        humidifier_bit = 0;
        msg.devctl |= (humidifier_bit << 3);
    }
    else
    {
        humidifier_bit = 1;
        msg.devctl |= (humidifier_bit << 3);
    }

    msgid_A = message_init("/home/linux", 'A');
    msgid_B = message_init("/home/linux", 'B');

    extract_name(cgiCookie, idbuf, sizeof(idbuf));
    msgsum = calculate_msgsum(idbuf);
    msg.msgtype = msgsum;
    msg.commd = 3;
    // printf("<!DOCTYPE html>");
    // printf("<html>");
    // printf("<body>");
    // printf("<center>");
    // printf("msg.devctl=%s\n",LED);
    // printf("msg.devctl=%s\n",FAN);
    // printf("msg.devctl=%s\n",BEEP);
    // printf("msg.devctl=%d\n",msg.devctl);
    // printf("</center>");
    // printf("</body>");
    // printf("</html>");
    msgsnd(msgid_A, &msg, sizeof(msg_t) - sizeof(long), 0);
    memset(&msg, 0, sizeof(msg_t));
    alarm(5);
    /*等待环境数据结果*/
    msgrcv(msgid_B, &msg, sizeof(msg_t) - sizeof(long), msgsum, 0);
    alarm(0);

    write_data_to_file(file_dev, LED);
    write_data_to_file(file_dev, FAN);
    write_data_to_file(file_dev, BEEP);

    if (fseek(file_dev, 0, SEEK_SET) != 0)
    {
        perror("Failed to seek in file");
        fclose(file_dev);
        return EXIT_FAILURE;
    }
    int j = 0;
    while (j < 3)
    {
        fgets(temp, sizeof(temp), file_dev);
        strcpy(buf[j], temp);
        j++;
    }
    size_t len;
    strcpy(LED, buf[0]);
    len = strlen(LED);
    if (len > 0 && LED[len - 1] == '\n')
    {
        LED[len - 1] = '\0';
    }
    strcpy(FAN, buf[1]);
    len = strlen(FAN);
    if (len > 0 && FAN[len - 1] == '\n')
    {
        FAN[len - 1] = '\0';
    }
    strcpy(BEEP, buf[2]);
    len = strlen(BEEP);
    if (len > 0 && BEEP[len - 1] == '\n')
    {
        BEEP[len - 1] = '\0';
    }

    // 写入JavaScript代码
    char javascriptCode[1024];
    snprintf(javascriptCode, sizeof(javascriptCode),
        "const data = {\n"
        "  LED: '%s',\n"
        "  FAN: '%s',\n"
        "  BEEP: '%s'\n"
        "};\n\n"
        "function setRadioValue(name, value) {\n"
        "  const box = document.getElementById('box4');\n"
        "  const radios = box.querySelectorAll(`input[name=\"${name}\"]`);\n"
        "  radios.forEach(radio => {\n"
        "    if (radio.value === value) {\n"
        "      radio.checked = true;\n"
        "      console.log(`${name} set to ${value}`);\n"  // 添加调试信息
        "    }\n"
        "  });\n"
        "}\n\n"
        "window.onload = function() {\n"
        "  setRadioValue('LED', data.LED);\n"
        "  setRadioValue('FAN', data.FAN);\n"
        "  setRadioValue('BEEP', data.BEEP);\n"
        "  console.log('All values are set');\n"  // 添加调试信息
        "};\n",
        LED, FAN, BEEP);
    fprintf(file_dev_js, "%s", javascriptCode);
    printf("Content-type: text/html;charset=\"UTF-8\"\n\n"); // 固定格式 必须要加
    printf("<script>window.location.href = '/greenhouse/garden_stuff.html';</script>");
    // printf("<script>window.location.href = '/greenhouse/text.html';</script>");
    //  关闭文件
    fclose(file_dev);
    fclose(file_dev_js);
    return 0;
}