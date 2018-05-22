#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/device.h>
#include <linux/slab.h>
static void sib_i2c_scan(struct device *dev, void *dummy)
{
    struct i2c_adapter *adap;
    struct i2c_dev *i2c_dev;
 
    if (dev->type != &i2c_adapter_type)
        	return ;
     
    adap = to_i2c_adapter(dev);
#if 0
    i2c_dev = get_free_i2c_dev(adap);
    if (IS_ERR(i2c_dev))
        return PTR_ERR(i2c_dev);
#endif
    pr_info("adapter_name:%s dev_name:%s\n", 
            adap->name, NULL/*i2c_dev->dev->name*/);
 
}
 
static int __init init_i2c_scan(void)
{
    struct i2c_adapter *adap = NULL;
    struct i2c_dev *i2c_dev = NULL;
 
    pr_info("Loading i2c scan\n");
    i2c_for_each_dev(NULL, sib_i2c_scan);
 
    return 0;
}
module_init(init_i2c_scan);
 
static void __exit remove_i2c_scan(void)
{
    printk("Unloading i2c scan\n");
}
module_exit(remove_i2c_scan);
 
MODULE_LICENSE("GPL");
