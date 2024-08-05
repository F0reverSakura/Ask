#ifndef __PROJECT_H__
#define __PROJECT_H__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>
#include <unistd.h>
#include "cgic.h"
#include <sqlite3.h>
#include <errno.h>
#include <pthread.h>

// 定义一些常量
#define MSGPATH "/home/linux/"
#define GETENV 1
#define SETENV 2
#define DEVCTL 3

typedef struct{
    char id[20];
    char ps[20];
    unsigned char flags;
} Login_t;

typedef struct{
   float temp;
   unsigned char hume;
   unsigned short lux;
   unsigned char devstuat; // 0 bit 照明设备   1-2 bit 温控设备   3 加湿器设备/  4-7 bit保留
} Env_t;

typedef struct{
    float uptemp;
    float downtemp;
    unsigned char uphume;
    unsigned char downhume;
    unsigned short uplux;
    unsigned short downlux;
} Limit_t;

typedef struct{
    long long msgtype;
    long long rcvtype;
    Login_t login;
    Env_t envdata;
    Limit_t limitdata;
    unsigned char devctl;
    unsigned char commd;
    
} msg_t;



#endif
