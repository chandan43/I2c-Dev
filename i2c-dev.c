/*  i2c-bus driver, char device interface.
 *  This driver is based on the 3.10.x version of drivers/i2c/i2c-dev.c 
 *  but has been rewritten to be easier to read and use.
 */

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

/*
 * An i2c_dev represents an i2c_adapter ... an I2C or SMBus master, not a
 * slave (i2c_client) with which messages will be exchanged.  It's coupled
 * with a character special file which is accessed by user mode drivers.
 *
 * The list of i2c_dev structures is parallel to the i2c_adapter lists
 * maintained by the driver model, and is updated using bus notifications.
 */
/*
 * i2c_adapter is the structure used to identify a physical i2c bus along
 * with the access algorithms necessary to access it.
 */
struct i2c_dev {
	struct list_head list;
	struct i2c_adapter *adap;
	struct device *dev;
	struct cdev cdev;
};
/* ------------------------------------------------------------------------- */

static struct class *i2c_dev_class;

static int i2cdev_attach_adapter(struct device *dev, void *dummy)
{
	struct i2c_adapter *adap;
	struct i2c_dev *i2c_dev;
	int res;

	if (dev->type != &i2c_adapter_type)
		return 0;
	adap = to_i2c_adapter(dev);
	i2c_dev = get_free_i2c_dev(adap); //TODO
	if (IS_ERR(i2c_dev))
		return PTR_ERR(i2c_dev);
	
	cdev_init(&i2c_dev->cdev, &i2cdev_fops);
	i2c_dev->cdev.owner = THIS_MODULE;
	res = cdev_add(&i2c_dev->cdev, MKDEV(I2C_MAJOR, adap->nr), 1);
	if (res)
		goto error_cdev;
	/* register this i2c device with the driver core */
	i2c_dev->dev = device_create(i2c_dev_class, &adap->dev,
				     MKDEV(I2C_MAJOR, adap->nr), NULL,
				     "i2c-%d", adap->nr);
	if (IS_ERR(i2c_dev->dev)) {
		res = PTR_ERR(i2c_dev->dev);
		goto error;
	}

	pr_debug("i2c-dev: adapter [%s] registered as minor %d\n",
		 adap->name, adap->nr);
	return 0;
error:
	cdev_del(&i2c_dev->cdev);
error_cdev:
	put_i2c_dev(i2c_dev);
	return res;	
}

static int i2cdev_detach_adapter(struct device *dev, void *dummy)
{
	struct i2c_adapter *adap;
	struct i2c_dev *i2c_dev;
	if (dev->type != &i2c_adapter_type)
		return 0;
	adap = to_i2c_adapter(dev);
	i2c_dev = i2c_dev_get_by_minor(adap->nr); //TODO:
	if (!i2c_dev) /* attach_adapter must have failed */
		return 0;
	cdev_del(&i2c_dev->cdev);
	put_i2c_dev(i2c_dev); //TODO
	device_destroy(i2c_dev_class, MKDEV(I2C_MAJOR, adap->nr));

	pr_debug("i2c-dev: adapter [%s] unregistered\n", adap->name);
	return 0;	

}
static int i2cdev_notifier_call(struct notifier_block *nb, unsigned long action,
			 void *data)
{
	struct device *dev = data;

	switch (action) {
	case BUS_NOTIFY_ADD_DEVICE:
		return i2cdev_attach_adapter(dev, NULL);
	case BUS_NOTIFY_DEL_DEVICE:
		return i2cdev_detach_adapter(dev, NULL);
	}

	return 0;
}

static struct notifier_block i2cdev_notifier = {
	.notifier_call = i2cdev_notifier_call,
};
/* ------------------------------------------------------------------------- */

/*
 * module load/unload record keeping
 */

static int __init i2c_dev_init(void)
{
	int res;
	pr_info("i2c /dev entries driver\n");
	
	res = register_chrdev(I2C_MAJOR, "i2c", &i2cdev_fops);
	if (res)
		goto out;
	i2c_dev_class = class_create(THIS_MODULE, "i2c-dev");
	if (IS_ERR(i2c_dev_class)) {
		res = PTR_ERR(i2c_dev_class);
		goto out_unreg_chrdev;
	}
	/* Keep track of adapters which will be added or removed later */
	res = bus_register_notifier(&i2c_bus_type, &i2cdev_notifier);
	if (res)
		goto out_unreg_class;
	
	/*In fact is to traverse the I2C bus on the klist_devices list, 
          for each device, __process_new_driver.I2C scanning from Linux Kernel*/
	/* Bind to already existing adapters right away */
	i2c_for_each_dev(NULL, i2cdev_attach_adapter);

	return 0;
out_unreg_class:
	class_destroy(i2c_dev_class);
out_unreg_chrdev:
	unregister_chrdev(I2C_MAJOR, "i2c");
out:
	pr_info("%s: Driver Initialisation failed\n", __FILE__);
	return res;
}

static void  __exit i2c_dev_exit(void)
{
	bus_unregister_notifier(&i2c_bus_type, &i2cdev_notifier);
	i2c_for_each_dev(NULL, i2cdev_detach_adapter);
	class_destroy(i2c_dev_class);
	unregister_chrdev_region(MKDEV(I2C_MAJOR, 0), I2C_MINORS);
}


MODULE_AUTHOR("Chandan Jha <beingchandanjha@gmail.com>");
MODULE_DESCRIPTION("I2C /dev entries driver");
MODULE_LICENSE("GPL");

module_init(i2c_dev_init);
module_exit(i2c_dev_exit);
