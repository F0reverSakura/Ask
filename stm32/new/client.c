#include "project.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <head.h>
#include <sys/ioctl.h>
#include "si7006.h"
#include "myled.h"

#define SERVER_IP "192.168.100.210"
#define BUFFER_SIZE 1024
msg_t buf_temp;
pthread_t tid;
int sockfd;
void send_login(int sockfd, const char *userid, const char *password)
{
    msg_t msg;
    memset(&msg, 0, sizeof(msg));

    strncpy(msg.login.id, userid, sizeof(msg.login.id) - 1);
    strncpy(msg.login.ps, password, sizeof(msg.login.ps) - 1);
    msg.commd = 0; // 0 代表登录命令

    if (send(sockfd, &msg, sizeof(msg), 0) < 0)
    {
        perror("send");
        exit(-1);
    }
    memset(&msg, 0, sizeof(msg_t));
    if (recv(sockfd, &msg, sizeof(msg), 0) < 0)
    {
        perror("send");
        exit(-1);
    }
    else
    {
        // 登陆成功
        if (msg.login.flags == 1)
        {
            printf("登陆成功\n");
        }
        else
        {
            exit(-1);
        }
    }
}
/*获取环境数据线程*/
void *envgetthread(void *argv)
{
    msg_t buf;
    memcpy(&buf, (msg_t *)argv, sizeof(msg_t));
    printf("envget :msg.commd=%d\n", buf.commd);

    int fd;
    int serial, firmware, tmp_code, hum_code;
    float tmp, hum;
    fd = open("/dev/si7006", O_RDWR);

    // ioctl(fd, GET_SERIAL, &serial);
    // ioctl(fd, GET_FIRMWARE, &firmware);
    // printf("serial = %#x,firmware = %#x\n", serial, firmware);

    ioctl(fd, GET_TMP, &tmp_code);
    ioctl(fd, GET_HUM, &hum_code);
    tmp = 175.72 * tmp_code / 65536 - 46.85;
    hum = 125.0 * hum_code / 65536 - 6;
    printf("tmp = %.2f,hum = %.2f\n", tmp, hum);

    close(fd);

    buf.envdata.temp = tmp;
    buf.envdata.hume = hum;
    buf.envdata.lux = 350;
    buf.envdata.devstuat = 0;
    send(sockfd, &buf, sizeof(msg_t), 0);
}

/*设置环境阈值线程*/
void *envsetthread(void *argv)
{
    msg_t buf = *(msg_t *)argv;
    printf("msg.commd=%d\n", buf.commd);
    send(sockfd, &buf, sizeof(msg_t), 0);
}

/*设备控制线程*/
void *devctlthread(void *argv)
{
    msg_t buf = *(msg_t *)argv;
    printf("msg.commd=%d\n", buf.commd);
    int fd;
    int which = 0;
    (fd = open("/dev/myled", O_RDWR));
    int light = buf.devctl & 0x01;
    int fan = (buf.devctl >> 1) & 0x03;
    int humidifier = (buf.devctl >> 3) & 0x01;
    printf("light=%d",light);
    if (light==0)
    {
        ioctl(fd, LED_OFF, &which);
    }else if (light==1)
    {
        ioctl(fd, LED_ON, &which);
    }
    send(sockfd, &buf, sizeof(msg_t), 0);
}

/*根据用户阈值环境维护线程*/
void *holdenvthread(void *argv)
{
    while (1)
    {
        sleep(1);
    }
}
int main(int argc, char *argv[])
{
    msg_t threadbuf;
    if (argc != 4)
    {
        fprintf(stderr, "Usage: %s <port> <user_id> <password>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    const char *user_id = argv[2];
    const char *password = argv[3];

    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    int n;

    // 创建套接字
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 设置服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0)
    {
        perror("Invalid address/ Address not supported");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // 连接服务器
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Connection failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    // 发送登录消息
    send_login(sockfd, user_id, password);

    while (1)
    {
        memset(&buf_temp, 0, sizeof(msg_t));
        if (0 == recv(sockfd, &buf_temp, sizeof(msg_t), 0))
        {
            return -1;
        }
        threadbuf = buf_temp;
        switch (threadbuf.commd)
        {
        case 1:
            pthread_create(&tid, NULL, envgetthread, &threadbuf);
            pthread_detach(tid);
            break;
        case 2:
            pthread_create(&tid, NULL, envsetthread, &threadbuf);
            pthread_detach(tid);
            break;
        case 3:
            pthread_create(&tid, NULL, devctlthread, &threadbuf);
            pthread_detach(tid);
            break;
        case 255:
            send(sockfd, &buf_temp, sizeof(msg_t), 0);
            break;
        default:
            send(sockfd, &buf_temp, sizeof(msg_t), 0);
            break;
        }
    }

    close(sockfd);
    return 0;
}