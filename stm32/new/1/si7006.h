#ifndef __SI7006_H__
#define __SI7006_H__
#define SERIAL_ADDR 0xfcc9
#define FIRMWARE_ADDR 0x84b8
#define TMP_ADDR 0xe3
#define HUM_ADDR 0xe5
#define GET_SERIAL _IOR('k',0,int)
#define GET_FIRMWARE _IOR('k',1,int)
#define GET_TMP _IOR('k',2,int)
#define GET_HUM _IOR('k',3,int)
#endif