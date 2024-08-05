#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>

typedef struct VideoBuffer {
	void   *start;
	size_t  length;
} VideoBuffer;
VideoBuffer *buffers;

int camera_device_open(const char * dev)
{
	int fd;
	//用阻塞模式打开摄像头设备
	fd = open(dev,O_RDWR);
	if(fd < 0){
		printf("open %s is fail.\n",dev);
		exit(EXIT_FAILURE);
	}
	return fd;
}

int init_camera_attribute(int fd)
{
	int numBufs;
	 struct v4l2_capability cap;
	struct v4l2_format fmt;
	struct v4l2_requestbuffers  req;
	struct v4l2_buffer    buf;

	//检查它是否是摄像头设备
	ioctl(fd,VIDIOC_QUERYCAP,&cap);
	if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
		return -1;
	
	//设置视频捕获格式
	memset(&fmt,0,sizeof(fmt));
	fmt.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width = 640;
	fmt.fmt.pix.height = 480;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
//	fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

	if(ioctl(fd,VIDIOC_S_FMT,&fmt) == -1){
		perror("set VIDIOC_S_FMT is fail");
		exit(EXIT_FAILURE);
	}
	
	//分配内存
	memset(&req,0,sizeof(req));
	req.count = 4;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;
	
	if(ioctl(fd,VIDIOC_REQBUFS,&req) == -1){
		perror("set VIDIOC_REQBUFS is fail");
		exit(EXIT_FAILURE);
	}

	//获取并记录缓存的物理空间
	buffers = calloc(req.count,sizeof(*buffers));
	for(numBufs = 0; numBufs < req.count; numBufs ++){
		memset(&buf,0,sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = numBufs;

		//读取缓存
		if(ioctl(fd,VIDIOC_QUERYBUF,&buf) == -1){
			perror("set VIDIOC_REQBUFS is fail");
			exit(EXIT_FAILURE);
		}
		 // 转换成相对地址
		buffers[numBufs].length = buf.length;
		buffers[numBufs].start  = mmap(NULL,buf.length,PROT_READ|PROT_WRITE,
				MAP_SHARED,fd,buf.m.offset);
		if(buffers[numBufs].start == MAP_FAILED){
			perror("mmap is fail");
			exit(EXIT_FAILURE);	
		}

		// 放入缓存队列
		if(ioctl(fd,VIDIOC_QBUF,&buf) == -1){
			perror("set VIDIOC_QBUF is fail");
			exit(EXIT_FAILURE);
		}
	}
	
	return 0;
}

int start_capturing(int fd)
{
	enum v4l2_buf_type type;

	//开始采集数据
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(ioctl(fd,VIDIOC_STREAMON,&type) == -1){
		perror("start capturing is fail");
		exit(EXIT_FAILURE);
	}

	return 0;
}

int build_picture(void *addr,int length)
{
	FILE *fp;
	static int num=0;
	char picture_name[20];
	sprintf(picture_name,"picture%d.yuv",num++);
	
	fp = fopen(picture_name,"w");
	if(fp == NULL){
		perror("fail to open ");
		exit(EXIT_FAILURE);
	}

	fwrite(addr,1,length,fp);

	fclose(fp);

	return 0;
}

int read_image(int fd)
{
	struct v4l2_buffer buf;
	memset(&buf,0,sizeof(buf));
	buf.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory=V4L2_MEMORY_MMAP;
	buf.index=0;

	//读取缓存
	if(ioctl(fd,VIDIOC_DQBUF,&buf) == -1){
		perror("set VIDIOC_DQBUF is fail");
		exit(EXIT_FAILURE);
	}//将数据存为图片
	build_picture(buffers[buf.index].start,buffers[buf.index].length);

	//重新放入缓存队列
	if(ioctl(fd,VIDIOC_QBUF,&buf) == -1){
		perror("reset VIDIOC_QBUF is fail");
		exit(EXIT_FAILURE);
	}

	return 0;

}

int when_to_read(int fd)
{
	int i=0;
	for(i=0;i<6;i++)
	{
		fd_set rfds;
		struct timeval tv;
		int retval;

		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);

		tv.tv_sec = 2;
		tv.tv_usec = 0;

		retval = select(fd+1, &rfds, NULL, NULL, &tv);
		if(retval == -1){
			 perror("select()");
			 exit(EXIT_FAILURE);
		}else if(retval == 0){
			printf("select is timeout\n");
		}else{
			read_image(fd);
		}
	}

	return 0;
}

int stop_capturing(int fd)
{
	enum v4l2_buf_type type;

	//开始采集数据
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(ioctl(fd,VIDIOC_STREAMOFF,&type) == -1){
		perror("stop capturing is fail");
		exit(EXIT_FAILURE);
	}

	return 0;
}

int uninit_camera(int fd)
{
	int i;
	for(i=0;i<4;i++){
		if(-1 == munmap(buffers[i].start,buffers[i].length))
		{
			perror("munmap is fail");
			exit(EXIT_FAILURE);
		}
	}
	free(buffers);
	
	close(fd);
	return 0;
}

int main(int argc, const char *argv[])
{
	int fd;
	fd = camera_device_open(argv[1]);	
	
	init_camera_attribute(fd);
	
	start_capturing(fd);
	
	when_to_read(fd);

	stop_capturing(fd);
	
	uninit_camera(fd);

	return 0;
}
