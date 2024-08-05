#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include "fs_max30102.h"
#define     CNAME       "fs_max30102"
#define     COUNT           1
dev_t devno;
struct cdev *cdev;
struct class *cls;
struct device *cls_dev;

struct i2c_client *max30102_client;
struct work_struct work;
int condition = 0;
struct wait_queue_head wq_head;
int i2c_write_msg(u8 reg, u8 data){

    int ret;
    u8 w_buf[] = { reg, data };
    struct i2c_msg w_msg = {
        .addr = max30102_client->addr,
        .flags = 0,
        .len = 2,
        .buf = w_buf,
    };

    ret = i2c_transfer(max30102_client->adapter, &w_msg, 1);
    if(1 != ret){
        printk("i2c_transfer error\n");
        return -EAGAIN;
    }
    printk("iic write reg=[%x] data=[%x] success\n", reg, data);
    return 0;
}

u8 i2c_read_msg_one_byte(u8 reg){
    int ret;
    u8 data = 0;
    u8 r_buf[] = { reg };
    struct i2c_msg r_msg[] = {
        [0] = {
            .addr = max30102_client->addr,
            .flags = 0,
            .len = 1,
            .buf = r_buf,
        },
        [1] = {
            .addr = max30102_client->addr,
            .flags = 1,
            .len = 1,
            .buf = &data,
        },
    };
    ret = i2c_transfer(max30102_client->adapter, r_msg, ARRAY_SIZE(r_msg));
    if(ARRAY_SIZE(r_msg) != ret){
        printk("i2c_transfer error\n");
        return -EAGAIN;
    }
    // printk("iic read data=[%x]success\n", data);
    return data;
}

u64 i2c_read_msg_mul_byte(u8 reg){
    int ret;
    u8 data[6] = {0};
    // u64 r_data[2] = {0};  //RED IR
    u8 r_buf[] = { reg };
    struct i2c_msg r_msg[] = {
        [0] = {
            .addr = max30102_client->addr,
            .flags = 0,
            .len = 1,
            .buf = r_buf,
        },
        [1] = {
            .addr = max30102_client->addr,
            .flags = 1,
            .len = 6,
            .buf = data,
        },
    };
    ret = i2c_transfer(max30102_client->adapter, r_msg, ARRAY_SIZE(r_msg));
    if(ARRAY_SIZE(r_msg) != ret){
        printk("i2c_transfer error\n");
        return -EAGAIN;
    }
    // printk("data[0]-data[5] = [%x %x %x %x %x %x]\n", data[0], data[1], data[2], data[3], data[4], data[5]);
    // data[0] &= 0x03;
    // data[3] &= 0x03;
    // printk("iic read reg=[%x]success\n", reg);
    // r_data[0] =  (long long)data[0] << 16 | (long long)data[1] << 8 | data[2];
    // r_data[1] =  (long long)data[3] << 16 | (long long)data[4] << 8 | data[5];
    // printk("data = %llx\n", r_data[1] << 32 | r_data[0]);
    return ((u64)data[0] << 40 | (u64)data[1] << 32 | (u64)data[2] << 24 | (u64)data[3] << 16 | (u64)data[4] << 8 | (u64)data[5]);
}


void max30102_work(struct work_struct *work){
    //开启中断阻塞  清除中断标志位
    condition = 1;
    wake_up_interruptible(&wq_head);
}

irqreturn_t max30102_irq_handler(int irq, void *dev){
    schedule_work(&work);
    return IRQ_HANDLED;
}

int fs_max30102_open(struct inode *inode, struct file *file){
    printk("%s %s %d\n", __FILE__, __func__, __LINE__);
    return 0;
}
int fs_max30102_close(struct inode *inode, struct file *file){
    printk("%s %s %d\n", __FILE__, __func__, __LINE__);
    return 0;
}
ssize_t fs_max30102_read (struct file *file, char __user *ubuf, size_t size, loff_t *offs){
    int ret = 0;
    u64 data = 0;
    u8 temp_data=0;
    if(file->f_flags & O_NONBLOCK){
        return -EINVAL;
    }else{
        i2c_read_msg_one_byte(Interrupt_Status1);
        ret = wait_event_interruptible(wq_head, condition);
        if(ret){
            printk("interrupt by signal\n");
            return -EINVAL;
        }
    }
    // printk("BLOCK interruptible\n");
    //read FIFO  1.RED channel 2.IR channel
    data = i2c_read_msg_mul_byte(FIFO4);
    if(size > sizeof(data)) size = sizeof(data);
    // printk("data = %#llx\n", data);
    while((temp_data&0x40)!=0x40){	
        temp_data = i2c_read_msg_one_byte(Interrupt_Status1);
    }
    ret = copy_to_user(ubuf, &data, size);
    if(ret){
        printk("copy_to_user failed\n");
        return -EFAULT;
    }
    condition = 0;
    return size;
}

// ssize_t fs_max30102_read (struct file *file, char __user *ubuf, size_t size, loff_t *offs){
//     int ret = 0;
//     int i = 0;
//     u8 data[6] = {0};
//     // u32 data_two[2] = {0};
//     u64 transfer_data = 0;
//     u8 fifo_wr_ptr = 0;
//     u8 temp_data=0;
//     if(file->f_flags & O_NONBLOCK){
//         return -EINVAL;
//     }else{
//         // printk("i2c read interrupt status\n");
//         i2c_read_msg_one_byte(Interrupt_Status1);
//         ret = wait_event_interruptible(wq_head, condition);
//         if(ret){
//             printk("interrupt by signal\n");
//             return -EINVAL;
//         }
//     }
//     // printk("BLOCK interruptible\n");
//     fifo_wr_ptr = i2c_read_msg_one_byte(FIFO1);
//     // printk("fifo_wr_ptr = %#x\n", fifo_wr_ptr);
//     //read FIFO  1.RED channel 2.IR channel
//     while((temp_data&0x40)!=0x40)
//     {	
//         temp_data = i2c_read_msg_one_byte(Interrupt_Status1);
//     }
//     for(i = 0; i < 6; i++){
//         i2c_read_msg_one_byte(Interrupt_Status1);
//         i2c_read_msg_one_byte(Interrupt_Status2);
//         data[i] = i2c_read_msg_one_byte(FIFO4);
//     }
//     // i2c_read_msg_mul_byte(FIFO4);
//     if(size > sizeof(data)) size = sizeof(data);
//     // printk("data[0]-data[5] = [%#x %#x %#x %#x %#x %#x]\n", data[0], data[1], data[2], data[3], data[4], data[5]);
//     data[0] &= 0x03;
//     data[3] &= 0x03;
// #if 1
//     transfer_data = transfer_data | (((long long)data[0]) << 40);
//     // printk("KS data = [%llx]\t", transfer_data);
//     transfer_data = transfer_data | (((long long)data[1]) << 32);
//     // printk("KS data = [%llx]\t", transfer_data);
//     transfer_data = transfer_data | (((long long)data[2]) << 24);
//     // printk("KS data = [%llx]\t", transfer_data);
//     transfer_data = transfer_data | (((long long)data[3]) << 16);
//     // printk("KS data = [%llx]\t", transfer_data);
//     transfer_data = transfer_data | (((long long)data[4]) << 8);
//     // printk("KS data = [%llx]\t", transfer_data);
//     transfer_data = transfer_data | ((long long)data[5]);
//     // printk("KS data = [%llx]\t", transfer_data);
//     // printk("KS RED data = [%llx]\t", (transfer_data & 0xffffff));
//     // printk("KS IR  data = [%llx]\n", (transfer_data & 0xffffff000000) >> 24);
// #else
//     data_two[0] = data[0] << 16 | data[1] << 8 | data[2];
//     data_two[1] = data[3] << 16 | data[4] << 8 | data[5];
//     printk("KS data_two[0] = [%x]\n", data_two[0]);      
//     printk("KS data_two[1] = [%x]\n", data_two[1]);       
// #endif
//     ret = copy_to_user(ubuf, &transfer_data, size);
//     if(ret){
//         printk("copy_to_user failed\n");
//         return -EFAULT;
//     }
//     condition = 0;
//     return size;
// }

ssize_t fs_max30102_write (struct file *file, const char __user *ubuf, size_t size, loff_t *offs){
    return size;
}
const struct file_operations fops = {
    .open = fs_max30102_open,
    .read = fs_max30102_read,
    .release = fs_max30102_close,
};


int fs_max30102_probe(struct i2c_client *client, const struct i2c_device_id *id){
    //匹配成功后会自动填充client结构体
    int ret;
    int i;
    max30102_client = client;
    printk("march ok, fs_ap3216c_probe\n");
    INIT_WORK(&work, max30102_work);  //初始化工作队列
    printk("max30102 init\n");

    i2c_write_msg(Mode_Configuration, 0x40); //reset     
    mdelay(50); //复位后延时一段时间
    i2c_write_msg(Interrupt_Enable1, 0x40);  //使能A_FULL_EN
    i2c_write_msg(Interrupt_Enable2, 0x00);  
    i2c_write_msg(FIFO1, 0x00);
    i2c_write_msg(FIFO2, 0x00);
    i2c_write_msg(FIFO3, 0x00);
    i2c_write_msg(FIFO_Configuration, 0x1F); //FIFO
    i2c_write_msg(Mode_Configuration, 0x03); //设置MOD Multisum LED --02 RED  --03 RED+IR
    i2c_write_msg(SpO2_Configuration, 0x27);
    i2c_write_msg(LED_Pulse_Amplitude1, 0x24);
    i2c_write_msg(LED_Pulse_Amplitude2, 0x24);
    // i2c_write_msg(Multi_LED_Mode_Control_Registers1, 0x7F);

    i2c_read_msg_one_byte(Interrupt_Status1);  //读取Interrupt status
    i2c_read_msg_one_byte(Interrupt_Status2);  //读取Interrupt status

    printk("client->irq = [%d]\n", client->irq);
    ret = request_irq(client->irq, max30102_irq_handler, IRQF_TRIGGER_FALLING, CNAME, NULL);
    if(ret){
        printk("request_irq error\n");
        goto err;
    }
    cdev = cdev_alloc();
    if(NULL == cdev){
        printk("cdev_alloc error\n");
        ret = -ENOMEM;
        goto err1;
    }
    cdev_init(cdev, &fops);
    ret = alloc_chrdev_region(&devno, 0, COUNT, CNAME);
    if(ret){
        printk("alloc_chrdev_region error\n");
        goto err2;
    }
    ret = cdev_add(cdev, devno, COUNT);
    if(ret){
        printk("cdev_add error\n");
        goto err3;
    }
    cls = class_create(THIS_MODULE, CNAME);
    if(IS_ERR(cls)){
        printk("class_create error\n");
        ret = PTR_ERR(cls);
        goto err4;
    }
    for(i=0;i<COUNT;i++){
        cls_dev = device_create(cls, NULL, devno, NULL, CNAME);
        if(IS_ERR(cls_dev)){
            printk("device_create error\n");
            ret = PTR_ERR(cls_dev);
            goto err5;
        }
    }
    init_waitqueue_head(&wq_head);
    return 0;
err5:
    class_destroy(cls);
err4:
    cdev_del(cdev);
err3:
    unregister_chrdev_region(devno, COUNT);
err2:
    kfree(cdev);
err1:
    free_irq(max30102_client->irq, NULL);
err:
    return ret;
}
int fs_max30102_remove(struct i2c_client *client){
    printk("%s %s %d\n", __FILE__, __func__, __LINE__);
    i2c_write_msg(Mode_Configuration, 0x40); //reset     
    device_destroy(cls, devno);
    class_destroy(cls);
    cdev_del(cdev);
    unregister_chrdev_region(devno, COUNT);
    kfree(cdev);
    free_irq(max30102_client->irq, NULL);
    return 0;
}

const struct of_device_id oftable[] = {
    {
        .compatible = "hqyj,max30102",
    },
    {},
};

struct i2c_driver fs_max30102 = {
    .probe = fs_max30102_probe,
    .remove = fs_max30102_remove,
    .driver = {
        .name = "fs_max30102",
        .of_match_table = oftable,
    }
};

module_i2c_driver(fs_max30102);
MODULE_LICENSE("GPL");
