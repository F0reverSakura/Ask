#include "myled.h"
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
// myleds{
//     led1 = <&gpioe 10 0>; //10gpioe中的管脚序号  0默认状态
//     led2 = <&gpiof 10 0>;
//     led3 = <&gpioe 8 0>;
// };
#define CNAME "myled"
struct device_node* node;
int gpiono;
int major;
struct class* cls;
struct device* dev;
int myled_open(struct inode* inode, struct file* file)
{
    printk("%s:%s:%d\n", __FILE__, __func__, __LINE__);
    return 0;
}
long myled_ioctl(struct file* file, unsigned int cmd, unsigned long arg)
{
    switch (cmd) {
    case LED_ON:
        gpio_set_value(gpiono, 1);
        break;
    case LED_OFF:
        gpio_set_value(gpiono, 0);
        break;
    }
    return 0;
}
int myled_close(struct inode* inode, struct file* file)
{
    printk("%s:%s:%d\n", __FILE__, __func__, __LINE__);
    return 0;
}
struct file_operations fops = {
    .open = myled_open,
    .unlocked_ioctl = myled_ioctl,
    .release = myled_close,
};

static int __init myled_init(void)
{
    int ret;
    // 1.获取设备节点
    node = of_find_node_by_name(NULL, "myleds");
    if (node == NULL) {
        printk("of_find_node_by_name error\n");
        return -ENODATA;
    }
    // 2.解析到gpio号
    gpiono = of_get_named_gpio(node, "led2", 0);
    if (gpiono < 0) {
        printk("of_get_named_gpio error\n");
        return gpiono;
    }
    // 3.申请要使用的gpio
    ret = gpio_request(gpiono, NULL);
    if (ret) {
        printk("gpio_request error\n");
        return ret;
    }
    // 4.设置方向,设置管脚输出的电平
    gpio_direction_output(gpiono, 1);

    // 5.注册字符设备驱动
    major = register_chrdev(0, CNAME, &fops);
    if (major < 0) {
        printk("register_chrdev error\n");
        return major;
    }
    // 6.自动创建设备节点
    cls = class_create(THIS_MODULE, CNAME);
    if (IS_ERR(cls)) {
        printk("class_create error\n");
        return PTR_ERR(cls);
    }
    dev = device_create(cls, NULL, MKDEV(major, 0), NULL, "%s", CNAME);
    if (IS_ERR(dev)) {
        printk("device_create error\n");
        return PTR_ERR(dev);
    }
    return 0;
}
static void __exit myled_exit(void)
{
    device_destroy(cls, MKDEV(major, 0));
    class_destroy(cls);
    unregister_chrdev(major, CNAME);
    gpio_set_value(gpiono, 0);
    gpio_free(gpiono);
}
module_init(myled_init);
module_exit(myled_exit);
MODULE_LICENSE("GPL");