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
#ifdef CONFIG_COMPAT

struct i2c_smbus_ioctl_data32 {
	u8 read_write;
	u8 command;
	u32 size;
	compat_caddr_t data; /* union i2c_smbus_data *data */
};

struct i2c_msg32 {
	u16 addr;
	u16 flags;
	u16 len;
	compat_caddr_t buf;
};

struct i2c_rdwr_ioctl_data32 {
	compat_caddr_t msgs; /* struct i2c_msg __user *msgs */
	u32 nmsgs;
};

/**
 * memdup_user - duplicate memory region from user space
 *
 * @src: source address in user space
 * @len: number of bytes to copy
 *
 * Returns an ERR_PTR() on failure.
 */
static long i2cdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct i2c_client *client = file->private_data;
	unsigned long funcs;
		
	dev_dbg(&client->adapter->dev, "ioctl, cmd=0x%02x, arg=0x%02lx\n",
		cmd, arg);
	switch(cmd) {
	case I2C_SLAVE: 
	case I2C_SLAVE_FORCE:
		if ((arg > 0x3ff) ||
		 (((client->flags & I2C_M_TEN) == 0) && arg > 0x7f))
			return -EINVAL;
		if (cmd == I2C_SLAVE &&  i2cdev_check_addr(client->adapter, arg))// TODO:
			return -EBUSY;
		/* REVISIT: address could become busy later */
		client->addr = arg;
		return 0;
	case I2C_TENBIT:
		if (arg)
			client->flags |= I2C_M_TEN;
		else
			client->flags &= ~I2C_M_TEN;
		return 0;
	case I2C_PEC: /* != 0 to use PEC with SMBus */
		/*
		 * Setting the PEC flag here won't affect kernel drivers,
		 * which will be using the i2c_client node registered with
		 * the driver model core.  Likewise, when that client has
		 * the PEC flag already set, the i2c-dev driver won't see
		 * (or use) this setting.
		 */
		if (arg)
			client->flags |= I2C_CLIENT_PEC;  /*Use Packet Error Checking*/
		else
			client->flags &= ~I2C_CLIENT_PEC;
		return 0;
	case I2C_FUNCS:
		funcs = i2c_get_functionality(client->adapter);
		return put_user(funcs, (unsigned long __user *)arg);
	case I2C_RDWR: {
		struct i2c_rdwr_ioctl_data rdwr_arg;
		struct i2c_msg *rdwr_pa;

		if (copy_from_user(&rdwr_arg,
				   (struct i2c_rdwr_ioctl_data __user *)arg,
				   sizeof(rdwr_arg)))
			return -EFAULT;

		/* Put an arbitrary limit on the number of messages that can
		 * be sent at once */
		if (rdwr_arg.nmsgs > I2C_RDWR_IOCTL_MAX_MSGS)
			return -EINVAL;

		rdwr_pa = memdup_user(rdwr_arg.msgs,
				      rdwr_arg.nmsgs * sizeof(struct i2c_msg));
		if (IS_ERR(rdwr_pa))
			return PTR_ERR(rdwr_pa);

		return i2cdev_ioctl_rdwr(client, rdwr_arg.nmsgs, rdwr_pa);
	}
	case I2C_SMBUS: {
		struct i2c_smbus_ioctl_data data_arg;
		if (copy_from_user(&data_arg,
				   (struct i2c_smbus_ioctl_data __user *) arg,
				   sizeof(struct i2c_smbus_ioctl_data)))
			return -EFAULT;
		return i2cdev_ioctl_smbus(client, data_arg.read_write, //TODO
					  data_arg.command,
					  data_arg.size,
					  data_arg.data);
	}
	case I2C_RETRIES:
		client->adapter->retries = arg;
		break;
	case I2C_TIMEOUT:
		/* For historical reasons, user-space sets the timeout
		 * value in units of 10 ms.
		 */
		client->adapter->timeout = msecs_to_jiffies(arg * 10);
		break;
	default:
		/* NOTE:  returning a fault code here could cause trouble
		 * in buggy userspace code.  Some old kernel bugs returned
		 * zero in this case, and userspace code might accidentally
		 * have depended on that bug.
		 */
		return -ENOTTY;
	}
	return 0;		
}

/*
  If this method exists, it will be called (without the BKL) whenever 
  a 32-bit process calls ioctl() on a 64-bit system. It should then 
  do whatever is required to convert the argument to native data types 
  and carry out the request. 
*/
/**
 * put_user: - Write a simple value into user space.
 * @x:   Value to copy to user space.
 * @ptr: Destination address, in user space.
 *
 * Context: User context only. This function may sleep if pagefaults are
 *          enabled.
 *
 * This macro copies a single simple value from kernel space to user
 * space.  It supports simple types like char and int, but not larger
 * data types like structures or arrays.
 *
 * @ptr must have pointer-to-simple-variable type, and @x must be assignable
 * to the result of dereferencing @ptr.
 *
 * Returns zero on success, or -EFAULT on error.
 */
long  compat_i2cdev_ioctl(struct file *, unsigned int cmd, unsigned long arg)
{
	struct i2c_client *client = file->private_data;
	unsigned long funcs;
	switch(cmd) {
	case I2C_FUNCS:
		funcs = i2c_get_functionality(client->adapter);	
		return put_user(funcs, (compat_ulong_t __user *)arg);
	case I2C_RDWR: {
		struct i2c_rdwr_ioctl_data32 rdwr_arg;
		struct i2c_msg32 *p;
		struct i2c_msg *rdwr_pa;
		int i;
		
		if (copy_from_user(&rdwr_arg,
				   (struct i2c_rdwr_ioctl_data32 __user *)arg,
				   sizeof(rdwr_arg)))
			return -EFAULT;
		if (rdwr_arg.nmsgs > I2C_RDWR_IOCTL_MAX_MSGS)
			return -EINVAL;
		rdwr_pa = kmalloc_array(rdwr_arg.nmsgs, sizeof(struct i2c_msg),
				      GFP_KERNEL);
		if (!rdwr_pa)
			return -ENOMEM;
		p = compat_ptr(rdwr_arg.msgs);
		for (i = 0; i < rdwr_arg.nmsgs; i++) {
			struct i2c_msg32 umsg;
			if (copy_from_user(&umsg, p + i, sizeof(umsg))) {
				kfree(rdwr_pa);
				return -EFAULT;
			}
			rdwr_pa[i] = (struct i2c_msg) {
				.addr = umsg.addr,
				.flags = umsg.flags,
				.len = umsg.len,
				.buf = compat_ptr(umsg.buf)
			};
		}
		return i2cdev_ioctl_rdwr(client, rdwr_arg.nmsgs, rdwr_pa); //TODO
	}
	case I2C_SMBUS: {
		struct i2c_smbus_ioctl_data32	data32;
		if (copy_from_user(&data32,
				   (void __user *) arg,
				   sizeof(data32)))
			return -EFAULT;
		return i2cdev_ioctl_smbus(client, data32.read_write,
					  data32.command,
					  data32.size,
					  compat_ptr(data32.data)); //TODO
	}
	default:
		return i2cdev_ioctl(file, cmd, arg);
	}		
}
#else
#define compat_i2cdev_ioctl NULL
#endif

static int i2cdev_open(struct inode *inode, struct file *file)
{
	unsigned int minor = iminor(inode);
	struct i2c_client *client;
	struct i2c_adapter *adap;

	adap = i2c_get_adapter(minor);
	if (!adap)
		return -ENODEV;
	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client) {
		i2c_put_adapter(adap);
		return -ENOMEM;
	}
	snprintf(client->name, I2C_NAME_SIZE, "i2c-dev %d", adap->nr);

	client->adapter = adap;
	file->private_data = client;

	return 0;
}
static int i2cdev_release(struct inode *inode, struct file *file)
{
	struct i2c_client *client = file->private_data;
		
	i2c_put_adapter(client->adapter);
	kfree(client);
	file->private_data = NULL;
	
	return 0;	
}
static const struct file_operations i2cdev_fops = {
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.read		= i2cdev_read,
	.write		= i2cdev_write,
	.unlocked_ioctl	= i2cdev_ioctl,
	.compat_ioctl	= compat_i2cdev_ioctl,
	.open		= i2cdev_open,
	.release	= i2cdev_release,
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
