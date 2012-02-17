/*
* Copyright (C) 2012 Prashant Shah, pshah.mumbai@gmail.com
* Copyright (C) 2012 Silicon Labs, Inc. (www.silabs.com)
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/*
 * Silicon Labs Si700x USB Evaluation Board driver.
 * http://www.silabs.com
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/ioctl.h>
#include <linux/mutex.h>

#include "si700x.h"

/* Usb transfer request */
struct transfer_req {
	u8 type;
	u8 status;
	u8 address;
	u8 length;
	u8 data[4];
} __attribute__ ((__packed__));

struct si700x_dev {
	struct usb_device *udev;		/* the usb device for this device */
	struct usb_interface *interface;	/* the interface for this device */
	struct transfer_req buffer;
	int buffer_size;
	int buffer_status;
	struct mutex lock;
};
#define to_dev(d) container_of(d, struct si700x_dev, kref)

static struct usb_driver si700x_driver;

static int si700x_open(struct inode *i, struct file *f)
{
	struct si700x_dev *dev;
	struct usb_interface *interface;
	int minor;

	pr_debug("Si700x: %s\n", __func__);

	minor = iminor(i);

	interface = usb_find_interface(&si700x_driver, minor);
	if (!interface) {
		printk(KERN_ERR "Si700x: failed to find interface for minor %d", minor);
		return -ENODEV;
	}

	dev = usb_get_intfdata(interface);
	if (!dev) {
		printk(KERN_ERR "Si700x: failed to find device for minor %d\n", minor);
		return -ENODEV;
	}

	/* save our object in the file's private structure */
	mutex_lock(&dev->lock);
	f->private_data = dev;
	mutex_unlock(&dev->lock);
	return 0;
}

static int si700x_release(struct inode *i, struct file *f)
{
	struct si700x_dev *dev;

	pr_debug("Si700x: %s\n", __func__);

	dev = (struct si700x_dev *)f->private_data;
	if (dev == NULL) {
		printk(KERN_ERR "Si700x: failed to find device from interface\n");
		return -ENODEV;
	}
	mutex_lock(&dev->lock);
	f->private_data = NULL;
	mutex_unlock(&dev->lock);
	return 0;
}

/* 
 * USB read function reads from the device with the address of the
 * si700x_dev.buffer which was filled by the USB write function previously.
 */
static ssize_t si700x_read(struct file *f, char __user *user_buffer, size_t count, loff_t *ppos)
{
	struct si700x_dev *dev;
	int retval = 0;
	int actual_length = 0;

	pr_debug("Si700x: %s\n", __func__);

	dev = (struct si700x_dev *)f->private_data;

	/* check the size of the data buffer */
	if (count != dev->buffer_size) {
		printk(KERN_ERR "Si700x: invalid buffer size, it should be %d bytes\n", dev->buffer_size);
		return -EFAULT;
	}

	/* check buffer status of the previous write function */
	if (dev->buffer_status != 1) {
		printk(KERN_ERR "Si700x: previous URB write was not successfull\n");
		return -EFAULT;
	}

	/* check access to user space buffer */
	if (!access_ok(VERIFY_WRITE, user_buffer, dev->buffer_size)) {
		printk(KERN_ERR "Si700x: invalid user space data\n");
		return -EFAULT;
	}

	mutex_lock(&dev->lock);
	dev->buffer_status = 0;

	retval = usb_bulk_msg(dev->udev,
		usb_rcvbulkpipe(dev->udev, PIPE_DATA_IN),
		&dev->buffer, dev->buffer_size,	// buffer, buffer length
		&actual_length, 0);		// bytes written, interval
	if (retval < 0) {
		printk(KERN_ERR "Si700x: failed to read URB\n");
		mutex_unlock(&dev->lock);
		return retval;
	}

	/* check if the read status is ok */
	if (dev->buffer.status != XFER_STATUS_SUCCESS) {
		printk(KERN_ERR "Si700x: device returned error status %d\n", dev->buffer.status);
		mutex_unlock(&dev->lock);
		return -EFAULT;
	}

	/* saving the original data in the buffer for the USB read function */
	if (copy_to_user(user_buffer, &dev->buffer, dev->buffer_size)) {
		printk(KERN_ERR "Si700x: failed to copy data to user space\n");
		mutex_unlock(&dev->lock);
		return -EFAULT;
	}

	dev->buffer_status = 1;
	mutex_unlock(&dev->lock);

	return actual_length;
}

/*
 * USB write function writes to the device and store the written data in the
 * si700x_dev.buffer which is used by the USB read function
 */
static ssize_t si700x_write(struct file *f, const char __user *user_buffer, size_t count, loff_t *ppos)
{
	struct si700x_dev *dev;
	int retval = 0;
	int actual_length = 0;

	pr_debug("Si700x: %s\n", __func__);

	dev = (struct si700x_dev *)f->private_data;

	/* check the size of the data buffer */
	if (count != dev->buffer_size) {
		printk(KERN_ERR "Si700x: invalid buffer size, it should be %d bytes\n", dev->buffer_size);
		return -EFAULT;
	}

	/* check access to user space buffer */
	if (!access_ok(VERIFY_READ, user_buffer, dev->buffer_size)) {
		printk(KERN_ERR "Si700x: invalid user space data\n");
		return -EFAULT;
	}

	mutex_lock(&dev->lock);
	/* saving the original data in the buffer for the USB read function */
	if (copy_from_user(&dev->buffer, user_buffer, dev->buffer_size)) {
		printk(KERN_ERR "Si700x: failed to copy data from user space\n");
		mutex_unlock(&dev->lock);
		return -EFAULT;
	}

	dev->buffer_status = 0;

	retval = usb_bulk_msg(dev->udev,
		usb_sndbulkpipe(dev->udev, PIPE_DATA_OUT),
		&dev->buffer, dev->buffer_size,	// buffer, buffer length
		&actual_length, 0);		// bytes written, interval
	if (retval < 0) {
		printk(KERN_ERR "Si700x: failed to write URB\n");
		mutex_unlock(&dev->lock);
		return retval;
	}

	dev->buffer_status = 1;
	mutex_unlock(&dev->lock);

	return actual_length;
}

static long si700x_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	struct si700x_dev *dev;
	int retval = 0;
	u16 version = 0;
	u8 port_count = 0;
	u8 board_id = 0;
	u16 port_id = 0;

	pr_debug("Si700x: %s\n", __func__);

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

	mutex_lock(&dev->lock);
	switch (cmd) {

	case SI700X_LED_ON:
		/* turn on the LED */
		retval = usb_control_msg(dev->udev,
			usb_sndctrlpipe(dev->udev, 0),
			REQ_SET_LED, CMD_VEN_DEV_OUT,		// request, request type 
			1, 0,					// value, index
			NULL, 0, 0);				// data, size, timeout
		if (retval < 0) {
			printk(KERN_ERR "Si700x: failed to turn ON the LED\n");
			goto error;
		}
		mutex_unlock(&dev->lock);
		return 0;

	case SI700X_LED_OFF:
		/* turn off the LED */
		retval = usb_control_msg(dev->udev,
			usb_sndctrlpipe(dev->udev, 0),
			REQ_SET_LED, CMD_VEN_DEV_OUT,		// request, request type 
			0, 0,					// value, index
			NULL, 0, 0);				// data, size, timeout
		if (retval < 0) {
			printk(KERN_ERR "Si700x: failed to turn OFF the LED\n");
			goto error;
		}
		mutex_unlock(&dev->lock);
		return 0;

	case SI700X_VERSION:
		/* read vesion number from the board */
		retval = usb_control_msg(dev->udev,
			usb_rcvctrlpipe(dev->udev, 0),
			REQ_GET_VERSION, CMD_VEN_DEV_IN,	// request, request type 
			0, 0,					// value, index
			&version, 2, 0);			// data, size, timeout
		if (retval < 0) {
			printk(KERN_ERR "Si700x: failed to read version number\n");
			goto error;
		}
		mutex_unlock(&dev->lock);
		return __put_user(version, (u16 __user *)arg);

	case SI700X_PORT_COUNT:
		/* read port count from the board */
		retval = usb_control_msg(dev->udev,
			usb_rcvctrlpipe(dev->udev, 0),
			REQ_GET_PORT_COUNT, CMD_VEN_DEV_IN,	// request, request type 
			0, 0,					// value, index
			&port_count, 1, 0);			// data, size, timeout
		if (retval < 0) {
			printk(KERN_ERR "Si700x: failed to read port count\n");
			goto error;
		}
		mutex_unlock(&dev->lock);
		return __put_user(port_count, (u8 __user *)arg);

	case SI700X_BOARDID:
		/* read board id from the board */
		retval = usb_control_msg(dev->udev,
			usb_rcvctrlpipe(dev->udev, 0),
			REQ_GET_BOARD_ID, CMD_VEN_DEV_IN,	// request, request type 
			0, 0,					// value, index
			&board_id, 1, 0);			// data, size, timeout
		if (retval < 0) {
			printk(KERN_ERR "Si700x: failed to read board id\n");
			goto error;
		}
		mutex_unlock(&dev->lock);
		return __put_user(board_id, (u8 __user *)arg);

	case SI700X_SETPROG_ON:
		/* turn on programming */
		retval = usb_control_msg(dev->udev,
			usb_sndctrlpipe(dev->udev, 0),
			REQ_SET_PROG, CMD_VEN_DEV_OUT,		// request, request type 
			1, 0,					// value, index - port
			NULL, 0, 0);				// data, size, timeout
		if (retval < 0) {
			printk(KERN_ERR "Si700x: failed to turn ON the programming mode\n");
			goto error;
		}
		mutex_unlock(&dev->lock);
		return 0;

	case SI700X_SETPROG_OFF:
		/* turn off programming */
		retval = usb_control_msg(dev->udev,
			usb_sndctrlpipe(dev->udev, 0),
			REQ_SET_PROG, CMD_VEN_DEV_OUT,		// request, request type 
			0, 0,					// value, index - port
			NULL, 0, 0);				// data, size, timeout
		if (retval < 0) {
			printk(KERN_ERR "Si700x: failed to turn OFF the programming mode\n");
			goto error;
		}
		mutex_unlock(&dev->lock);
		return 0;

	case SI700X_SETSLEEP_ON:
		port_id = arg;
		/* turn on sleeping */
		retval = usb_control_msg(dev->udev,
			usb_sndctrlpipe(dev->udev, 0),
			REQ_SET_SLEEP, CMD_VEN_DEV_OUT,		// request, request type 
			0, port_id,				// value, index - port
			NULL, 0, 0);				// data, size, timeout
		if (retval < 0) {
			printk(KERN_ERR "Si700x: failed to turn ON the sleeping for port %d\n", port_id);
			goto error;
		}
		mutex_unlock(&dev->lock);
		return 0;

	case SI700X_SETSLEEP_OFF:
		port_id = arg;
		/* turn off sleeping */
		retval = usb_control_msg(dev->udev,
			usb_sndctrlpipe(dev->udev, 0),
			REQ_SET_SLEEP, CMD_VEN_DEV_OUT,		// request, request type 
			0, port_id,				// value, index - port
			NULL, 0, 0);				// data, size, timeout
		if (retval < 0) {
			printk(KERN_ERR "Si700x: failed to turn OFF the sleeping for port %d\n", port_id);
			goto error;
		}
		mutex_unlock(&dev->lock);
		return 0;

	}
	mutex_unlock(&dev->lock);
	return -EINVAL;
error:
	mutex_unlock(&dev->lock);
	return retval;
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

	pr_debug("Si700x: %s\n", __func__);
	
	dev = kmalloc(sizeof(struct si700x_dev), GFP_KERNEL);
	if (!dev) {
		printk(KERN_ERR "Si700x: failed to allocate memory for device\n");
		return -ENOMEM;
	}
	memset(dev, 0x00, sizeof(dev));

	mutex_init(&dev->lock);
	mutex_lock(&dev->lock);
	dev->interface = interface;
	dev->udev = interface_to_usbdev(interface);
	dev->buffer_size = sizeof(dev->buffer);

	iface_desc = interface->cur_altsetting;
	
	usb_set_intfdata(interface, dev);

	retval = usb_register_dev(interface, &si700x_class);
	if (retval < 0) {
		printk(KERN_ERR "Si700x: failed to get minor number\n");
		usb_set_intfdata(interface, NULL);
		mutex_unlock(&dev->lock);
		kfree(dev);
		return retval;
	}
	mutex_unlock(&dev->lock);
	printk(KERN_INFO "Si700x: minor number obtained %d\n", interface->minor);
	return 0;
}

static void si700x_disconnect(struct usb_interface *interface)
{
	struct si700x_dev *dev;
	int minor = interface->minor;

	pr_debug("Si700x: %s\n", __func__);

	dev = usb_get_intfdata(interface);
	mutex_lock(&dev->lock);
	usb_deregister_dev(interface, &si700x_class);
	usb_set_intfdata(interface, NULL);
	mutex_unlock(&dev->lock);
	kfree(dev);
	printk(KERN_INFO "Si700x: USB #%d now disconnted\n", minor);
}

static struct usb_device_id si700x_table[] = {
	{ USB_DEVICE(0x10c4,0x8649) },
	{}
};
MODULE_DEVICE_TABLE(usb, si700x_table);

static struct usb_driver si700x_driver = {
	.name = "si700x",
	.probe = si700x_probe,
	.disconnect = si700x_disconnect,
	.id_table = si700x_table,
};

static int __init si700x_init(void)
{
	int retval;

	pr_debug("Si700x: %s\n", __func__);

	retval = usb_register(&si700x_driver);
	if (retval) {
		printk(KERN_ERR "Si700x: failed to register usb device\n");
		return retval;
	}
	return 0;
}

static void __exit si700x_exit(void)
{
	pr_debug("Si700x: %s\n", __func__);

	usb_deregister(&si700x_driver);
}

module_init(si700x_init);
module_exit(si700x_exit);

MODULE_AUTHOR("Prashant Shah <pshah.mumbai@gmail.com>");
MODULE_DESCRIPTION("Silicon Labs Si700x USB Evaluation Board");
MODULE_LICENSE("GPL v2");

