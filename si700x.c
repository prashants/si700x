#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/ioctl.h>

#include "si700x.h"

/* Pipes */
#define PIPE_DATA_OUT      0x02
#define PIPE_DATA_IN       0x82

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

/* Usb transfer request */
struct transfer_req {
	u8 type;
	u8 status;
	u8 address;
	u8 length;
	u8 data[4];
} __attribute__ ((__packed__));

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
		printk(KERN_ERR "si700x: failed to find device for minor %d", minor);
		return -ENODEV;
	}

	dev = usb_get_intfdata(interface);
	if (!dev) {
		printk(KERN_ERR "si700x: failed to get device from interface\n");
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
		printk(KERN_ERR "si700x: failed to find device from interface\n");
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

static void completion_callback(struct urb *urb)
{
	printk(KERN_INFO "si700x: %s\n", __func__);
}


static int transferI2C(struct transfer_req *t, struct file *f)
{
	struct si700x_dev *dev;
	struct urb *urb;
	int retval = 0;

	printk(KERN_INFO "si700x: %s\n", __func__);

	dev = (struct si700x_dev *)f->private_data;

	urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!urb) {
		printk(KERN_ERR "si700x: failed to allocate URB\n");
		return -ENOMEM;
	}
	usb_fill_int_urb(urb, dev->udev,
		 usb_sndintpipe(dev->udev, PIPE_DATA_OUT),
		 t, sizeof(struct transfer_req),		// buffer, buffer length
		 completion_callback,				// completion function
		 NULL, 1);					// content, interval
	retval = usb_submit_urb(urb, GFP_KERNEL);
	if (retval < 0) {
		printk(KERN_ERR "si700x: failed to submit write URB %d\n", retval);
		return retval;
	}
	msleep(100);
	printk(KERN_INFO "si700x: URB write status %d\n", urb->status);

	usb_fill_int_urb(urb, dev->udev,
		 usb_rcvintpipe(dev->udev, PIPE_DATA_IN),
		 t, sizeof(struct transfer_req),		// buffer, buffer length
		 completion_callback,				// completion function
		 NULL, 1);					// content, interval
	retval = usb_submit_urb(urb, GFP_ATOMIC);
	if (retval < 0) {
		printk(KERN_ERR "si700x: failed to submit read URB %d\n", retval);
		return retval;
	}
	msleep(100);
	printk(KERN_INFO "si700x: URB read status %d\n", urb->status);

	usb_free_urb(urb);
	return retval;
}

static ssize_t si700x_write(struct file *f, const char __user *user_buffer, size_t count, loff_t *ppos)
{
	struct si700x_dev *dev;
	int retval = 0;
	struct transfer_req transfer_buf;
	u8 counter = 0;
	u16 temp = 0;

	printk(KERN_INFO "si700x: %s\n", __func__);

	dev = (struct si700x_dev *)f->private_data;

	printk(KERN_INFO "*********** START TEMPERATURE **************\n");

	/* transfer */
	transfer_buf.type = XFER_TYPE_WRITE;
	transfer_buf.status = 0x00;	// status
	transfer_buf.address = 0x42;	// slave id
	transfer_buf.length = 0x01;	// max is 4 bytes
	transfer_buf.data[0] = REG_CFG2;	// register
	transfer_buf.data[1] = CFG2_EN_TEST_REG;	// register
	transfer_buf.data[2] = 0x00;	// register
	transfer_buf.data[3] = 0x00;	// register

	retval = transferI2C(&transfer_buf, f);
	if (retval < 0) {
		printk(KERN_ERR "si700x: failed to transfer I2C packet\n");
		return retval;
	}
	printk(KERN_INFO "si700x: **** counter %d\n", counter);
	printk(KERN_INFO "si700x: type %x\n", transfer_buf.type);
	printk(KERN_INFO "si700x: status %x\n", transfer_buf.status);
	printk(KERN_INFO "si700x: address %x\n", transfer_buf.address);
	printk(KERN_INFO "si700x: length %d\n", transfer_buf.length);
	printk(KERN_INFO "si700x: data1 %x\n", transfer_buf.data[0]);
	printk(KERN_INFO "si700x: data2 %x\n", transfer_buf.data[1]);
	printk(KERN_INFO "si700x: data3 %x\n", transfer_buf.data[2]);
	printk(KERN_INFO "si700x: data4 %x\n", transfer_buf.data[3]);

	if (transfer_buf.status == 0x02) {
		printk(KERN_ERR "si700x: not found at %X\n", transfer_buf.address);
		return -EFAULT;
	}
	if (transfer_buf.status == 0x01) {
		printk(KERN_ERR "si700x: found at %X\n", transfer_buf.address);
	}

	/* clear data */
	transfer_buf.type = XFER_TYPE_WRITE;
	transfer_buf.status = 0x00;	// status
	transfer_buf.address = 0x42;	// slave id
	transfer_buf.length = 0x02;	// max is 4 bytes
	transfer_buf.data[0] = REG_CFG1;	// register
	transfer_buf.data[1] = 0x00;	// register
	transfer_buf.data[2] = 0x00;	// register
	transfer_buf.data[3] = 0x00;	// register

	retval = transferI2C(&transfer_buf, f);
	if (retval < 0) {
		printk(KERN_ERR "si700x: failed to transfer I2C packet\n");
		return retval;
	}
	printk(KERN_INFO "si700x: **** clear %d\n", counter);
	printk(KERN_INFO "si700x: type %x\n", transfer_buf.type);
	printk(KERN_INFO "si700x: status %x\n", transfer_buf.status);
	printk(KERN_INFO "si700x: address %x\n", transfer_buf.address);
	printk(KERN_INFO "si700x: length %d\n", transfer_buf.length);
	printk(KERN_INFO "si700x: data1 %x\n", transfer_buf.data[0]);
	printk(KERN_INFO "si700x: data2 %x\n", transfer_buf.data[1]);
	printk(KERN_INFO "si700x: data3 %x\n", transfer_buf.data[2]);
	printk(KERN_INFO "si700x: data4 %x\n", transfer_buf.data[3]);

	/* write command data */
	transfer_buf.type = XFER_TYPE_WRITE;
	transfer_buf.status = 0x00;	// status
	transfer_buf.address = 0x42;	// slave id
	transfer_buf.length = 0x02;	// max is 4 bytes
	transfer_buf.data[0] = REG_CFG1;	// register
	transfer_buf.data[1] = 0x11;	// data
	transfer_buf.data[2] = 0x00;	// data
	transfer_buf.data[3] = 0x00;	// data

	retval = transferI2C(&transfer_buf, f);
	if (retval < 0) {
		printk(KERN_ERR "si700x: failed to transfer I2C packet\n");
		return retval;
	}
	printk(KERN_INFO "si700x: **** write %d\n", counter);
	printk(KERN_INFO "si700x: type %x\n", transfer_buf.type);
	printk(KERN_INFO "si700x: status %x\n", transfer_buf.status);
	printk(KERN_INFO "si700x: address %x\n", transfer_buf.address);
	printk(KERN_INFO "si700x: length %d\n", transfer_buf.length);
	printk(KERN_INFO "si700x: data1 %x\n", transfer_buf.data[0]);
	printk(KERN_INFO "si700x: data2 %x\n", transfer_buf.data[1]);
	printk(KERN_INFO "si700x: data3 %x\n", transfer_buf.data[2]);
	printk(KERN_INFO "si700x: data4 %x\n", transfer_buf.data[3]);

	msleep(10000);

	/* read status */
	transfer_buf.type = XFER_TYPE_WRITE_READ;
	transfer_buf.status = 0x00;	// status
	transfer_buf.address = 0x42;	// slave id
	transfer_buf.length = 0x01;	// max is 4 bytes
	transfer_buf.data[0] = REG_STATUS;	// register
	transfer_buf.data[1] = 0x00;	// data
	transfer_buf.data[2] = 0x00;	// data
	transfer_buf.data[3] = 0x00;	// data

	retval = transferI2C(&transfer_buf, f);
	if (retval < 0) {
		printk(KERN_ERR "si700x: failed to transfer I2C packet\n");
		return retval;
	}
	printk(KERN_INFO "si700x: **** status %d\n", counter);
	printk(KERN_INFO "si700x: type %x\n", transfer_buf.type);
	printk(KERN_INFO "si700x: status %x\n", transfer_buf.status);
	printk(KERN_INFO "si700x: address %x\n", transfer_buf.address);
	printk(KERN_INFO "si700x: length %d\n", transfer_buf.length);
	printk(KERN_INFO "si700x: data1 %x\n", transfer_buf.data[0]);
	printk(KERN_INFO "si700x: data2 %x\n", transfer_buf.data[1]);
	printk(KERN_INFO "si700x: data3 %x\n", transfer_buf.data[2]);
	printk(KERN_INFO "si700x: data4 %x\n", transfer_buf.data[3]);

	/* read data 0 */
	transfer_buf.type = XFER_TYPE_WRITE_READ;
	transfer_buf.status = 0x00;	// status
	transfer_buf.address = 0x42;	// slave id
	transfer_buf.length = 0x01;	// max is 4 bytes
	transfer_buf.data[0] = 0x01;	// register
	transfer_buf.data[1] = 0x00;	// data
	transfer_buf.data[2] = 0x00;	// data
	transfer_buf.data[3] = 0x00;	// data

	retval = transferI2C(&transfer_buf, f);
	if (retval < 0) {
		printk(KERN_ERR "si700x: failed to transfer I2C packet\n");
		return retval;
	}
	printk(KERN_INFO "si700x: **** data 1 %d\n", counter);
	printk(KERN_INFO "si700x: type %x\n", transfer_buf.type);
	printk(KERN_INFO "si700x: status %x\n", transfer_buf.status);
	printk(KERN_INFO "si700x: address %x\n", transfer_buf.address);
	printk(KERN_INFO "si700x: length %d\n", transfer_buf.length);
	printk(KERN_INFO "si700x: data1 %x\n", transfer_buf.data[0]);
	printk(KERN_INFO "si700x: data2 %x\n", transfer_buf.data[1]);
	printk(KERN_INFO "si700x: data3 %x\n", transfer_buf.data[2]);
	printk(KERN_INFO "si700x: data4 %x\n", transfer_buf.data[3]);

	temp = transfer_buf.data[0];
	temp = temp << 6;

	/* read data 1 */
	transfer_buf.type = XFER_TYPE_WRITE_READ;
	transfer_buf.status = 0x00;	// status
	transfer_buf.address = 0x42;	// slave id
	transfer_buf.length = 0x01;	// max is 4 bytes
	transfer_buf.data[0] = 0x02;	// register
	transfer_buf.data[1] = 0x00;	// data
	transfer_buf.data[2] = 0x00;	// data
	transfer_buf.data[3] = 0x00;	// data

	retval = transferI2C(&transfer_buf, f);
	if (retval < 0) {
		printk(KERN_ERR "si700x: failed to transfer I2C packet\n");
		return retval;
	}
	printk(KERN_INFO "si700x: **** data 2 %d\n", counter);
	printk(KERN_INFO "si700x: type %x\n", transfer_buf.type);
	printk(KERN_INFO "si700x: status %x\n", transfer_buf.status);
	printk(KERN_INFO "si700x: address %x\n", transfer_buf.address);
	printk(KERN_INFO "si700x: length %d\n", transfer_buf.length);
	printk(KERN_INFO "si700x: data1 %x\n", transfer_buf.data[0]);
	printk(KERN_INFO "si700x: data2 %x\n", transfer_buf.data[1]);
	printk(KERN_INFO "si700x: data3 %x\n", transfer_buf.data[2]);
	printk(KERN_INFO "si700x: data4 %x\n", transfer_buf.data[3]);

	transfer_buf.data[0] = transfer_buf.data[0] >> 2;
	temp = temp | transfer_buf.data[0];
	temp = (temp/32) - 50;
	printk(KERN_INFO "si700x: temperature %d\n", temp);

	printk(KERN_INFO "*********** START HUMIDITY **************\n");

	/* transfer */
	transfer_buf.type = XFER_TYPE_WRITE;
	transfer_buf.status = 0x00;	// status
	transfer_buf.address = 0x42;	// slave id
	transfer_buf.length = 0x01;	// max is 4 bytes
	transfer_buf.data[0] = REG_CFG2;	// register
	transfer_buf.data[1] = CFG2_EN_TEST_REG;	// register
	transfer_buf.data[2] = 0x00;	// register
	transfer_buf.data[3] = 0x00;	// register

	retval = transferI2C(&transfer_buf, f);
	if (retval < 0) {
		printk(KERN_ERR "si700x: failed to transfer I2C packet\n");
		return retval;
	}
	printk(KERN_INFO "si700x: **** counter %d\n", counter);
	printk(KERN_INFO "si700x: type %x\n", transfer_buf.type);
	printk(KERN_INFO "si700x: status %x\n", transfer_buf.status);
	printk(KERN_INFO "si700x: address %x\n", transfer_buf.address);
	printk(KERN_INFO "si700x: length %d\n", transfer_buf.length);
	printk(KERN_INFO "si700x: data1 %x\n", transfer_buf.data[0]);
	printk(KERN_INFO "si700x: data2 %x\n", transfer_buf.data[1]);
	printk(KERN_INFO "si700x: data3 %x\n", transfer_buf.data[2]);
	printk(KERN_INFO "si700x: data4 %x\n", transfer_buf.data[3]);

	if (transfer_buf.status == 0x02) {
		printk(KERN_ERR "si700x: not found at %X\n", transfer_buf.address);
		return -EFAULT;
	}
	if (transfer_buf.status == 0x01) {
		printk(KERN_ERR "si700x: found at %X\n", transfer_buf.address);
	}

	/* clear data */
	transfer_buf.type = XFER_TYPE_WRITE;
	transfer_buf.status = 0x00;	// status
	transfer_buf.address = 0x42;	// slave id
	transfer_buf.length = 0x02;	// max is 4 bytes
	transfer_buf.data[0] = REG_CFG1;	// register
	transfer_buf.data[1] = 0x00;	// register
	transfer_buf.data[2] = 0x00;	// register
	transfer_buf.data[3] = 0x00;	// register

	retval = transferI2C(&transfer_buf, f);
	if (retval < 0) {
		printk(KERN_ERR "si700x: failed to transfer I2C packet\n");
		return retval;
	}
	printk(KERN_INFO "si700x: **** clear %d\n", counter);
	printk(KERN_INFO "si700x: type %x\n", transfer_buf.type);
	printk(KERN_INFO "si700x: status %x\n", transfer_buf.status);
	printk(KERN_INFO "si700x: address %x\n", transfer_buf.address);
	printk(KERN_INFO "si700x: length %d\n", transfer_buf.length);
	printk(KERN_INFO "si700x: data1 %x\n", transfer_buf.data[0]);
	printk(KERN_INFO "si700x: data2 %x\n", transfer_buf.data[1]);
	printk(KERN_INFO "si700x: data3 %x\n", transfer_buf.data[2]);
	printk(KERN_INFO "si700x: data4 %x\n", transfer_buf.data[3]);

	/* write command data */
	transfer_buf.type = XFER_TYPE_WRITE;
	transfer_buf.status = 0x00;	// status
	transfer_buf.address = 0x42;	// slave id
	transfer_buf.length = 0x02;	// max is 4 bytes
	transfer_buf.data[0] = REG_CFG1;	// register
	transfer_buf.data[1] = 0x01;	// data
	transfer_buf.data[2] = 0x00;	// data
	transfer_buf.data[3] = 0x00;	// data

	retval = transferI2C(&transfer_buf, f);
	if (retval < 0) {
		printk(KERN_ERR "si700x: failed to transfer I2C packet\n");
		return retval;
	}
	printk(KERN_INFO "si700x: **** write %d\n", counter);
	printk(KERN_INFO "si700x: type %x\n", transfer_buf.type);
	printk(KERN_INFO "si700x: status %x\n", transfer_buf.status);
	printk(KERN_INFO "si700x: address %x\n", transfer_buf.address);
	printk(KERN_INFO "si700x: length %d\n", transfer_buf.length);
	printk(KERN_INFO "si700x: data1 %x\n", transfer_buf.data[0]);
	printk(KERN_INFO "si700x: data2 %x\n", transfer_buf.data[1]);
	printk(KERN_INFO "si700x: data3 %x\n", transfer_buf.data[2]);
	printk(KERN_INFO "si700x: data4 %x\n", transfer_buf.data[3]);

	msleep(10000);

	/* read status */
	transfer_buf.type = XFER_TYPE_WRITE_READ;
	transfer_buf.status = 0x00;	// status
	transfer_buf.address = 0x42;	// slave id
	transfer_buf.length = 0x01;	// max is 4 bytes
	transfer_buf.data[0] = REG_STATUS;	// register
	transfer_buf.data[1] = 0x00;	// data
	transfer_buf.data[2] = 0x00;	// data
	transfer_buf.data[3] = 0x00;	// data

	retval = transferI2C(&transfer_buf, f);
	if (retval < 0) {
		printk(KERN_ERR "si700x: failed to transfer I2C packet\n");
		return retval;
	}
	printk(KERN_INFO "si700x: **** status %d\n", counter);
	printk(KERN_INFO "si700x: type %x\n", transfer_buf.type);
	printk(KERN_INFO "si700x: status %x\n", transfer_buf.status);
	printk(KERN_INFO "si700x: address %x\n", transfer_buf.address);
	printk(KERN_INFO "si700x: length %d\n", transfer_buf.length);
	printk(KERN_INFO "si700x: data1 %x\n", transfer_buf.data[0]);
	printk(KERN_INFO "si700x: data2 %x\n", transfer_buf.data[1]);
	printk(KERN_INFO "si700x: data3 %x\n", transfer_buf.data[2]);
	printk(KERN_INFO "si700x: data4 %x\n", transfer_buf.data[3]);

	/* read data 0 */
	transfer_buf.type = XFER_TYPE_WRITE_READ;
	transfer_buf.status = 0x00;	// status
	transfer_buf.address = 0x42;	// slave id
	transfer_buf.length = 0x01;	// max is 4 bytes
	transfer_buf.data[0] = 0x01;	// register
	transfer_buf.data[1] = 0x00;	// data
	transfer_buf.data[2] = 0x00;	// data
	transfer_buf.data[3] = 0x00;	// data

	retval = transferI2C(&transfer_buf, f);
	if (retval < 0) {
		printk(KERN_ERR "si700x: failed to transfer I2C packet\n");
		return retval;
	}
	printk(KERN_INFO "si700x: **** data 1 %d\n", counter);
	printk(KERN_INFO "si700x: type %x\n", transfer_buf.type);
	printk(KERN_INFO "si700x: status %x\n", transfer_buf.status);
	printk(KERN_INFO "si700x: address %x\n", transfer_buf.address);
	printk(KERN_INFO "si700x: length %d\n", transfer_buf.length);
	printk(KERN_INFO "si700x: data1 %x\n", transfer_buf.data[0]);
	printk(KERN_INFO "si700x: data2 %x\n", transfer_buf.data[1]);
	printk(KERN_INFO "si700x: data3 %x\n", transfer_buf.data[2]);
	printk(KERN_INFO "si700x: data4 %x\n", transfer_buf.data[3]);

	temp = transfer_buf.data[0];
	temp = temp << 4;

	/* read data 1 */
	transfer_buf.type = XFER_TYPE_WRITE_READ;
	transfer_buf.status = 0x00;	// status
	transfer_buf.address = 0x42;	// slave id
	transfer_buf.length = 0x01;	// max is 4 bytes
	transfer_buf.data[0] = 0x02;	// register
	transfer_buf.data[1] = 0x00;	// data
	transfer_buf.data[2] = 0x00;	// data
	transfer_buf.data[3] = 0x00;	// data

	retval = transferI2C(&transfer_buf, f);
	if (retval < 0) {
		printk(KERN_ERR "si700x: failed to transfer I2C packet\n");
		return retval;
	}
	printk(KERN_INFO "si700x: **** data 2 %d\n", counter);
	printk(KERN_INFO "si700x: type %x\n", transfer_buf.type);
	printk(KERN_INFO "si700x: status %x\n", transfer_buf.status);
	printk(KERN_INFO "si700x: address %x\n", transfer_buf.address);
	printk(KERN_INFO "si700x: length %d\n", transfer_buf.length);
	printk(KERN_INFO "si700x: data1 %x\n", transfer_buf.data[0]);
	printk(KERN_INFO "si700x: data2 %x\n", transfer_buf.data[1]);
	printk(KERN_INFO "si700x: data3 %x\n", transfer_buf.data[2]);
	printk(KERN_INFO "si700x: data4 %x\n", transfer_buf.data[3]);

	transfer_buf.data[0] = transfer_buf.data[0] >> 4;
	temp = temp | transfer_buf.data[0];
	temp = (temp/16) - 24;
	printk(KERN_INFO "si700x: humidity %d\n", temp);


	return count;
}

static long si700x_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	u16 version = 0;
	u8 port_count = 0;
	u8 board_id = 0;
	u16 port_id = 0;
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

	case SI700X_SETPROG_ON:
		/* turn on programming */
		retval = usb_control_msg(dev->udev,
			usb_sndctrlpipe(dev->udev, 0),
			REQ_SET_PROG, CMD_VEN_DEV_OUT,		// request, request type 
			1, 0,					// value, index - port
			NULL, 0, 0);				// data, size, timeout
		if (retval < 0) {
			printk(KERN_ERR "si700x: failed to turn the programming ON\n");
			return retval;
		}
		return 0;

	case SI700X_SETPROG_OFF:
		/* turn off programming */
		retval = usb_control_msg(dev->udev,
			usb_sndctrlpipe(dev->udev, 0),
			REQ_SET_PROG, CMD_VEN_DEV_OUT,		// request, request type 
			0, 0,					// value, index - port
			NULL, 0, 0);				// data, size, timeout
		if (retval < 0) {
			printk(KERN_ERR "si700x: failed to turn the programming OFF\n");
			return retval;
		}
		return 0;

	case SI700X_SETSLEEP_ON:
		port_id = arg;
		printk(KERN_ERR "si700x: sleeping ON port %d\n", port_id);
		/* turn on sleeping */
		retval = usb_control_msg(dev->udev,
			usb_sndctrlpipe(dev->udev, 0),
			REQ_SET_SLEEP, CMD_VEN_DEV_OUT,		// request, request type 
			0, port_id,				// value, index - port
			NULL, 0, 0);				// data, size, timeout
		if (retval < 0) {
			printk(KERN_ERR "si700x: failed to turn the sleeping ON for port %d\n", port_id);
			return retval;
		}
		return 0;

	case SI700X_SETSLEEP_OFF:
		port_id = arg;
		printk(KERN_ERR "si700x: sleeping OFF port %d\n", port_id);
		/* turn off sleeping */
		retval = usb_control_msg(dev->udev,
			usb_sndctrlpipe(dev->udev, 0),
			REQ_SET_SLEEP, CMD_VEN_DEV_OUT,		// request, request type 
			0, port_id,				// value, index - port
			NULL, 0, 0);				// data, size, timeout
		if (retval < 0) {
			printk(KERN_ERR "si700x: failed to turn the sleeping OFF for port %d\n", port_id);
			return retval;
		}
		return 0;

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

