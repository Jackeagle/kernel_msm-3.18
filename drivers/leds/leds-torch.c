#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/freezer.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>


#define VIRCDEV_CLASS_NAME	"flashlight"
#define VIRCDEV_DEVICE_NAME	"torch"
#define GPIO_TORCH_PIN 	1002
#define FLASHLIGHT_ENABLE 1
#define FLASHLIGHT_DISABLE 0

struct class *g_vircdev_class;
static int torch_major = 555;
static int torch_minor = 0;

static ssize_t torch_enable_show(struct device *dev,struct device_attribute *attr,char *buf)
{
	int val = gpio_get_value(GPIO_TORCH_PIN);
	return sprintf(buf, "%d\n", val);
}

static ssize_t torch_enable_store(struct device *dev,struct device_attribute *attr,const char *buf,size_t size)
{
	unsigned int val;
	int ret;

	//val = simple_strtoul(buf, &after, 10);
	ret = kstrtouint(buf, 10, &val);
	if (ret != 0)
		return ret;

	if(val == 0)
	{
		gpio_direction_output(GPIO_TORCH_PIN, FLASHLIGHT_DISABLE);
	}
	else if(val == 1)
	{
		gpio_direction_output(GPIO_TORCH_PIN, FLASHLIGHT_ENABLE);
	}

	return size;
}

static DEVICE_ATTR(enable, S_IRUGO | S_IWUGO, torch_enable_show, torch_enable_store);

static int __init flash_light_init(void)
{
	struct device *dev = NULL;
	int err = 0;

	pr_info("flash-light driver: init\n");

	g_vircdev_class = class_create(THIS_MODULE, VIRCDEV_CLASS_NAME);
	if(IS_ERR(g_vircdev_class)) {
		err = PTR_ERR(g_vircdev_class);
		printk(KERN_ALERT "Failed to create class.\n");
		goto CLASS_CREATE_ERR;
	}

	dev = device_create(g_vircdev_class, NULL, MKDEV(torch_major,torch_minor), NULL, VIRCDEV_DEVICE_NAME);
	if(IS_ERR(dev)) {
		err = PTR_ERR(dev);
		printk(KERN_ALERT "Failed to create device.\n");
		goto DEVICE_CREATE_ERR;
	}

	err = device_create_file(dev, &dev_attr_enable);
	if(err < 0) {
		printk(KERN_ALERT"Failed to create attribute file.");
		goto DEVICE_CREATE_FILE_ERR;
	}

	return 0;

DEVICE_CREATE_FILE_ERR:
DEVICE_CREATE_ERR:
	device_destroy(g_vircdev_class, MKDEV(torch_major, torch_minor));
CLASS_CREATE_ERR:
	class_destroy(g_vircdev_class);
	return err;
}

static void __exit flash_light_exit(void)
{
	pr_info("flash-light driver: exit\n");
	device_destroy(g_vircdev_class, MKDEV(torch_major, torch_minor));
	class_destroy(g_vircdev_class);
}

module_init(flash_light_init);
module_exit(flash_light_exit);


MODULE_AUTHOR("BlueBank");
MODULE_DESCRIPTION("BlueBank Flash Light Driver");
MODULE_LICENSE("GPL");
