#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/ioctl.h>

#include "si700x.h"

#define INTERRUPT_EP_IN 0x82
#define INTERRUPT_EP_OUT 0x02

/* Request type */
#define CMD_VEN_DEV_OUT    0x40
#define CMD_VEN_DEV_IN     0xC0

/* Request */
#define REQ_GET_VERSION    0
#define REQ_SET_LED        1
#define REQ_SET_SLEEP      2
#define REQ_SET_PROG       3
#define REQ_GET_PORT_COUNT 4
#define REQ_GET_BOARD_ID   5

struct si700x_dev {
	struct usb_device *	udev;			/* the usb device for this device */
	struct usb_interface *	interface;		/* the interface for this device */
};
#define to_dev(d) container_of(d, struct si700x_dev, kref)

static struct usb_driver si700x_driver;

static int si700x_open(struct inode *i, struct file *f)
{
	struct si700x_dev *dev;
	struct usb_interface *interface;
	int minor;

	printk(KERN_INFO "si700x: %s\n", __func__);

	minor = iminor(i);

	interface = usb_find_interface(&si700x_driver, minor);
	if (!interface) {
		printk(KERN_ERR "si700x: error, can't find device for minor %d", minor);
		return -ENODEV;
	}

	dev = usb_get_intfdata(interface);
	if (!dev) {
		printk(KERN_ERR "si700x: can't find device from interface\n");
		return -ENODEV;
	}

	/* save our object in the file's private structure */
	f->private_data = dev;
	return 0;
}

static int si700x_release(struct inode *i, struct file *f)
{
	struct si700x_dev *dev;

	printk(KERN_INFO "si700x: %s\n", __func__);

	dev = (struct si700x_dev *)f->private_data;
	if (dev == NULL) {
		printk(KERN_ERR "si700x: can't find device from interface\n");
		return -ENODEV;
	}
	return 0;
}

static ssize_t si700x_read(struct file *f, char __user *user_buffer, size_t count, loff_t *ppos)
{
	struct si700x_dev *dev;

	printk(KERN_INFO "si700x: %s\n", __func__);

	dev = (struct si700x_dev *)f->private_data;

	return count;
}

static ssize_t si700x_write(struct file *f, const char __user *user_buffer, size_t count, loff_t *ppos)
{
	struct si700x_dev *dev;
	int retval = 0;
	struct transfer_req {
		char type;
		char status;
	        char address;
		char length;
		char data[4];
	} __attribute__ ((__packed__));
	struct transfer_req transfer_buf;
	int transfer_buf_size = sizeof(transfer_buf);

	printk(KERN_INFO "si700x: %s\n", __func__);

	dev = (struct si700x_dev *)f->private_data;

	/* transfer */
	transfer_buf.type = XFER_TYPE_READ;
	transfer_buf.status = 0x0;
	transfer_buf.address = 0x00;
	transfer_buf.length = 4;
	transfer_buf.data[0] = 0x00;

	retval = usb_bulk_msg(dev->udev,
		usb_sndbulkpipe(dev->udev, INTERRUPT_EP_OUT),
		&transfer_buf, transfer_buf_size,		// data, len 
		&count, 0);					// actual len, timeout
	if (retval < 0) {
		printk(KERN_ERR "si700x: failed to write from device\n");
		return retval;
	}
	retval = usb_bulk_msg(dev->udev,
		usb_rcvbulkpipe(dev->udev, INTERRUPT_EP_IN),
		&transfer_buf, transfer_buf_size,		// data, len 
		&count, HZ*5);					// actual len, timeout
	if (retval < 0) {
		printk(KERN_ERR "si700x: failed to read from device\n");
		return retval;
	}
	printk(KERN_INFO "si700x: data1 %c\n", transfer_buf.data[0]);
	printk(KERN_INFO "si700x: data2 %c\n", transfer_buf.data[1]);
	printk(KERN_INFO "si700x: data3 %c\n", transfer_buf.data[2]);
	printk(KERN_INFO "si700x: data4 %c\n", transfer_buf.data[3]);
	printk(KERN_INFO "si700x: status %c\n", transfer_buf.status);
	printk(KERN_INFO "si700x: count %d\n", count);

	/* check if slave is present */
	//printk(KERN_INFO "si700x: checking if slave present %d\n", port_count);
	//urb = usb_alloc_urb(0, GFP_KERNEL);
	//transfer_buffer = usb_alloc_coherent(dev->udev, 1, GFP_KERNEL, &urb->transfer_dma);
	//transfer_buffer_size = 1;
	//usb_fill_int_urb(urb, dev->udev,
	//	usb_rcvintpipe(dev->udev, INTERRUPT_EP_IN),
	//	transfer_buffer, transfer_buffer_size,
	//	slave_detect_complete,
	//	NULL, 0);
	//retval = usb_submit_urb(urb, GFP_KERNEL);
	//printk("si700x: transfer return value %d\n", retval);
	//msleep(500);
	//usb_free_coherent(dev->udev, 1, transfer_buffer, urb->transfer_dma);
	//usb_free_urb(urb);

	return count;
}

static long si700x_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	u16 version = 0;
	u8 port_count = 0;
	u8 board_id = 0;
	struct si700x_dev *dev;

	printk(KERN_INFO "si700x: %s\n", __func__);

	dev = (struct si700x_dev *)f->private_data;

	/* check if the ioctl is for the right device and within range */
	if (_IOC_NR(cmd) > SI700X_IOC_MAXNR)
		return -ENOTTY;

	/* check if the data read and write from user is allowed */
	if (_IOC_DIR(cmd) & _IOC_READ)
		retval = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		retval =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (retval)
		return -EFAULT;

	switch (cmd) {

	case SI700X_LED_ON:
		/* turn on the LED */
		retval = usb_control_msg(dev->udev,
			usb_sndctrlpipe(dev->udev, 0),
			REQ_SET_LED, CMD_VEN_DEV_OUT,		// request, request type 
			1, 0,					// value, index
			NULL, 0, 0);				// data, size, timeout
		if (retval < 0) {
			printk(KERN_ERR "si700x: failed to turn the LED ON\n");
			return retval;
		}
		return 0;

	case SI700X_LED_OFF:
		/* turn off the LED */
		retval = usb_control_msg(dev->udev,
			usb_sndctrlpipe(dev->udev, 0),
			REQ_SET_LED, CMD_VEN_DEV_OUT,		// request, request type 
			0, 0,					// value, index
			NULL, 0, 0);				// data, size, timeout
		if (retval < 0) {
			printk(KERN_ERR "si700x: failed to turn the LED OFF\n");
			return retval;
		}
		return 0;

	case SI700X_VERSION:
		/* read vesion number from the board */
		retval = usb_control_msg(dev->udev,
			usb_rcvctrlpipe(dev->udev, 0),
			REQ_GET_VERSION, CMD_VEN_DEV_IN,	// request, request type 
			0, 0,					// value, index
			&version, 2, 0);			// data, size, timeout
		if (retval < 0) {
			printk(KERN_ERR "si700x: failed to read version number\n");
			return retval;
		}
		return __put_user(version, (u16 __user *)arg);

	case SI700X_PORT_COUNT:
		/* read port count from the board */
		retval = usb_control_msg(dev->udev,
			usb_rcvctrlpipe(dev->udev, 0),
			REQ_GET_PORT_COUNT, CMD_VEN_DEV_IN,	// request, request type 
			0, 0,					// value, index
			&port_count, 1, 0);			// data, size, timeout
		if (retval < 0) {
			printk(KERN_ERR "si700x: failed to read port count\n");
			return retval;
		}
		return __put_user(port_count, (u8 __user *)arg);

	case SI700X_BOARDID:
		/* read board id from the board */
		retval = usb_control_msg(dev->udev,
			usb_rcvctrlpipe(dev->udev, 0),
			REQ_GET_BOARD_ID, CMD_VEN_DEV_IN,	// request, request type 
			0, 0,					// value, index
			&board_id, 1, 0);			// data, size, timeout
		if (retval < 0) {
			printk(KERN_ERR "si700x: failed to read board id\n");
			return retval;
		}
		return __put_user(board_id, (u8 __user *)arg);

	}
	return -EINVAL;
}

static struct file_operations si700x_fops = {
	.open = si700x_open,
	.release = si700x_release,
	.read = si700x_read,
	.write = si700x_write,
	.unlocked_ioctl = si700x_ioctl,
};

static struct usb_class_driver si700x_class = {
	.name = "si700x%d",
	.fops = &si700x_fops,
	.minor_base = 192,
};

static int si700x_probe(struct usb_interface *interface,
		const struct usb_device_id *id)
{
	struct si700x_dev *dev = NULL;
	struct usb_host_interface *iface_desc;
	int retval = -ENOMEM;

	printk(KERN_INFO "si700x: %s\n", __func__);
	
	dev = kmalloc(sizeof(struct si700x_dev), GFP_KERNEL);
	if (!dev) {
		printk(KERN_ERR "si700x: cannnot allocate memory for device structure\n");
		return -ENOMEM;
	}
	memset(dev, 0x00, sizeof(dev));
	dev->interface = interface;
	dev->udev = interface_to_usbdev(interface);

	iface_desc = interface->cur_altsetting;
	
	usb_set_intfdata(interface, dev);

	retval = usb_register_dev(interface, &si700x_class);
	if (retval < 0) {
		printk(KERN_ERR "si700x: not able to get minor for this device\n");
		usb_set_intfdata(interface, NULL);
		kfree(dev);
		return retval;
	}
	printk(KERN_INFO "si700x: minor obtained %d\n", interface->minor);
	return 0;
}

static void si700x_disconnect(struct usb_interface *interface)
{
	struct si700x_dev *dev;
	int minor = interface->minor;

	printk(KERN_INFO "si700x: %s\n", __func__);

	dev = usb_get_intfdata(interface);
	usb_deregister_dev(interface, &si700x_class);
	usb_set_intfdata(interface, NULL);
	kfree(dev);
	printk(KERN_INFO "si700x: USB #%d now disconnted\n", minor);
}

static struct usb_device_id si700x_table[] = {
	{ USB_DEVICE(0x10c4,0x8649) },
	{}
};
MODULE_DEVICE_TABLE(usn, si700x_table);

static struct usb_driver si700x_driver = {
	.name = "si700x",
	.probe = si700x_probe,
	.disconnect = si700x_disconnect,
	.id_table = si700x_table,
};

static int __init si700x_init(void)
{
	int retval;

	printk(KERN_INFO "si700x: %s\n", __func__);

	retval = usb_register(&si700x_driver);
	if (retval) {
		printk(KERN_ERR "si700x: failed to register the usb device\n");
		return retval;
	}
	return 0;
}

static void __exit si700x_exit(void)
{
	printk(KERN_INFO "si700x: %s\n", __func__);

	usb_deregister(&si700x_driver);
}

module_init(si700x_init);
module_exit(si700x_exit);

MODULE_AUTHOR("Prashant Shah <pshah.mumbai@gmail.com>");
MODULE_DESCRIPTION("Silicon Labs Si700x USB Evaluation Module");
MODULE_LICENSE("GPL");

