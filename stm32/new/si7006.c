#include "si7006.h"
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#define CNAME "si7006"
struct i2c_client* gclient;
int major;
struct class* cls;
struct device* dev;
int i2c_read_serial_firmware(u16 reg)
{
    int ret;
    u8 data;
    u8 r_buf[] = { reg >> 8, reg & 0xff };
    // 1.封装消息
    struct i2c_msg r_msg[] = {
        [0] = {
            .addr = gclient->addr,
            .flags = 0,
            .len = 2,
            .buf = r_buf,
        },
        [1] = {
            .addr = gclient->addr,
            .flags = 1,
            .len = 1,
            .buf = &data,
        }
    };
    // 2.发送消息
    ret = i2c_transfer(gclient->adapter, r_msg, 2);
    if (ret != 2) {
        printk("i2c_transfer failed\n");
        return -EIO;
    }
    return data;
}
int i2c_read_hum_tmp(u8 reg)
{
    int ret;
    u16 data;
    u8 r_buf[] = { reg };
    // 1.封装消息
    struct i2c_msg r_msg[] = {
        [0] = {
            .addr = gclient->addr,
            .flags = 0,
            .len = 1,
            .buf = r_buf,
        },
        [1] = {
            .addr = gclient->addr,
            .flags = 1,
            .len = 2,
            .buf = (u8*)&data,
        }
    };
    // 2.发送消息
    ret = i2c_transfer(gclient->adapter, r_msg, 2);
    if (ret != 2) {
        printk("i2c_transfer failed\n");
        return -EIO;
    }
    return data << 8 | data >> 8; 
}
int si7006_open(struct inode* inode, struct file* file)
{
    printk("%s:%s:%d\n", __FILE__, __func__, __LINE__);
    return 0;
}
long si7006_ioctl(struct file* file,
    unsigned int cmd, unsigned long arg)
{
    int data, ret;
    switch (cmd) {
    case GET_SERIAL:
        data = i2c_read_serial_firmware(SERIAL_ADDR);
        if (data < 0) {
            return data;
        }
        ret = copy_to_user((void*)arg, &data, sizeof(data));
        if (ret) {
            printk("copy_to_user failed\n");
            return -EFAULT;
        }
        break;
    case GET_FIRMWARE:
        data = i2c_read_serial_firmware(FIRMWARE_ADDR);
        if (data < 0) {
            return data;
        }
        ret = copy_to_user((void*)arg, &data, sizeof(data));
        if (ret) {
            printk("copy_to_user failed\n");
            return -EFAULT;
        }
        break;
    case GET_TMP:
        data = i2c_read_hum_tmp(TMP_ADDR); 
        data = data & 0xffff;
        ret = copy_to_user((void*)arg, &data, sizeof(data));
        if (ret) {
            printk("copy_to_user failed\n");
            return -EFAULT;
        }
        break;
    case GET_HUM:
        data = i2c_read_hum_tmp(HUM_ADDR);
        data = data & 0xffff;
        ret = copy_to_user((void*)arg, &data, sizeof(data));
        if (ret) {
            printk("copy_to_user failed\n");
            return -EFAULT;
        }
    }
    return 0;
}
int si7006_close(struct inode* inode, struct file* file)
{
    printk("%s:%s:%d\n", __FILE__, __func__, __LINE__);
    return 0;
}
struct file_operations fops = {
    .open = si7006_open,
    .unlocked_ioctl = si7006_ioctl,
    .release = si7006_close,
};
int si7006_probe(struct i2c_client* client,
    const struct i2c_device_id* id)
{
    gclient = client;
    printk("%s:%s:%d\n", __FILE__, __func__, __LINE__);
    // 1.注册字符设备驱动
    major = register_chrdev(0, CNAME, &fops);
    if (major < 0) {
        printk("register_chrdev failed\n");
        return major;
    }
    // 2.创建设备节点
    cls = class_create(THIS_MODULE, CNAME);
    if (IS_ERR(cls)) {
        printk("class_create failed\n");
        unregister_chrdev(major, CNAME);
        return PTR_ERR(cls);
    }
    dev = device_create(cls, NULL, MKDEV(major, 0), NULL, CNAME);
    if (IS_ERR(dev)) {
        printk("device_create failed\n");
        class_destroy(cls);
        unregister_chrdev(major, CNAME);
        return PTR_ERR(dev);
    }
    return 0;
}
int si7006_remove(struct i2c_client* client)
{
    printk("%s:%s:%d\n", __FILE__, __func__, __LINE__);
    device_destroy(cls, MKDEV(major, 0));
    class_destroy(cls);
    unregister_chrdev(major, CNAME);
    return 0;
}
struct of_device_id oftable[] = {
    { .compatible = "hqyj,si7006" },
    {},
};
struct i2c_driver si7006 = {
    .probe = si7006_probe,
    .remove = si7006_remove,
    .driver = {
        .name = "si7006",
        .of_match_table = oftable,
    },
};
module_i2c_driver(si7006);
MODULE_LICENSE("GPL");