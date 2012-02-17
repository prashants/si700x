Steps to install
----------------

The following steps have been tested on Ubuntu 11.10 with kernel
version 3.2.5.

Compile the kernel module by running the following commands :

$make
$sudo insmod ./si700x.ko

This will create a device file '/dev/si700x0' for communicating with the
device. The last integer '0' will vary depending on the number of devices
connected. It uses IOCTL calls to get and set parameters and read and write
calls to read and send commands/data to the device.

There is a test.c file included with the driver for testing the device.
Compile the test program by running :

$gcc test.c -o test
$sudo ./test
