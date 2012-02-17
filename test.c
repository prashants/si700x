#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "si700x.h"

/* Usb transfer request */
struct transfer_req {
	char type;
	char status;
	char address;
	char length;
	char data[4];
} __attribute__ ((__packed__));

int fd;			/* the deivce file */
int fast_conv = 0;
unsigned char board_address = 0x00;

int board_status(unsigned char);
int get_temperature(void);
int get_humidity(void);
void fast_conversion(int);
int heater(int);
unsigned char get_device_id();

#define DELAY sleep(1)

int main()
{
	short int version = 0;
	char port_count;
	char board_id;
	unsigned int port_id;
	unsigned char address = 0x00;
	unsigned char sensor_device_id;

	int temperature;
	int humidity;

	fd = open("/dev/si700x0", O_RDWR);
	if (fd < 0) {
		printf("Cannot open device file\n");
		return 1;
	}

	if (ioctl(fd, SI700X_LED_ON) == -1) {
		printf("Failed to ON the LED: %s\n", strerror(errno));
	} else {
		puts("LED is ON");
	}

	if (ioctl(fd, SI700X_VERSION, &version) == -1) {
		printf("Failed to read version: %s\n", strerror(errno));
	} else {
		printf("Version %d\n", version);
	}

	if (ioctl(fd, SI700X_PORT_COUNT, &port_count) == -1) {
		printf("Failed to read port count: %s\n", strerror(errno));
	} else {
		printf("Port Count %d\n", port_count);
	}

	if (ioctl(fd, SI700X_BOARDID, &board_id) == -1) {
		printf("Failed to read board id: %s\n", strerror(errno));
	} else {
		printf("Board ID %d\n", board_id);
	}

	if (ioctl(fd, SI700X_SETPROG_OFF) == -1) {
		printf("Failed to turn OFF programming: %s\n", strerror(errno));
	} else {
		printf("Programming is OFF\n");
	}

	for (port_id = 0; port_id < port_count; port_id++) {
		if (ioctl(fd, SI700X_SETSLEEP_OFF, port_id) == -1) {
			printf("Failed to OFF Sleeping for port %d: %s\n", port_id, strerror(errno));
		} else {
			printf("Sleeping is OFF for port %d\n", port_id);
		}
	}

	/* give time for the ports to wake up */
	sleep(10);

	// heater(0);
	fast_conversion(0);

	/* scan for board address */
	for (address = 0x40; address <= 0x43; address++) {
		if (board_status(address)) {
			board_address = address;
			printf("Board found at address 0x%X\n", board_address);
		}
	}
	if (!board_address) {
		printf("Board not found\n");
		return 0;
	}

	/* get sensor deivce ID */
	sensor_device_id = get_device_id();
	if (sensor_device_id < 0) {
		printf("Error reading sensor device ID\n");
	} else {
		printf("Device ID : %X\n", sensor_device_id);
	}

	DELAY;

	/* read temperature */
	temperature = get_temperature();
	if (temperature < 0) {
		printf("Error reading temperature\n");
	} else {
		printf("Current temperature is : %f\n", (((float)temperature / 32.0) - 50.0));
	}

	DELAY;

	/* read humidity */
	humidity = get_humidity();
	if (humidity < 0) {
		printf("Error reading humidity\n");
	} else {
		printf("Current humidity is : %f\n", (((float)humidity / 16.0) - 24.0));
	}

	if (ioctl(fd, SI700X_LED_OFF) == -1) {
		printf("Failed to OFF the LED: %s\n", strerror(errno));
	} else {
		printf("LED is OFF\n");
	}

	close(fd);
	return 0;
}

int board_status(unsigned char board_id)
{
	int retval = 0;
	struct transfer_req data;

	/* clear status */
	data.type = XFER_TYPE_WRITE;
	data.status = 0x00;
	data.address = board_id;
	data.length = 0x00;
	data.data[0] = REG_CFG1; 
	data.data[1] = 0x00;
	data.data[2] = 0x00;
	data.data[3] = 0x00;
	retval = write(fd, &data, sizeof(data));
	if (retval < 0) {
		printf("Error writing get board status command\n");
		return 0;
	}
	retval = read(fd, &data, sizeof(data));
	if (data.status == 1) {
		return 1;
	} else {
		return 0;
	}
}

int get_temperature()
{
	int counter = 0;
	unsigned short value = 0;
	unsigned short temp = 0;
	int retval = 0;

	struct transfer_req data;

	/* clear status */
	data.type = XFER_TYPE_WRITE;
	data.status = 0x00;
	data.address = board_address;
	data.length = 0x02;
	data.data[0] = REG_CFG1; 
	data.data[1] = 0x00;
	data.data[2] = 0x00;
	data.data[3] = 0x00;
	retval = write(fd, &data, sizeof(data));
	if (retval < 0) {
		printf("Error writing clear status command\n");
		return -1;
	}
	retval = read(fd, &data, sizeof(data));
	if (retval < 0) {
		printf("Error reading clear status command\n");
		return -1;
	}
	if (data.status != 1) {
		printf("Error clearing status\n");
		return -1;
	}

	DELAY;

	/* write temperature */
	data.type = XFER_TYPE_WRITE;
	data.status = 0x00;
	data.address = board_address;
	data.length = 0x02;
	data.data[0] = REG_CFG1;
	if (fast_conv == 0)
		data.data[1] = 0x11;
	else
		data.data[1] = 0x20 | 0x11;
	data.data[1] = 0x11;
	data.data[2] = 0x00;
	data.data[3] = 0x00;
	retval = write(fd, &data, sizeof(data));
	if (retval < 0) {
		printf("Error writing get temperature command\n");
		return -1;
	}
	retval = read(fd, &data, sizeof(data));
	if (retval < 0) {
		printf("Error reading get temperature command\n");
		return -1;
	}
	if (data.status != 1) {
		printf("Error writing temparature command\n");
		return -1;
	}

	DELAY;

	/* check status */
	while (1) {
		data.type = XFER_TYPE_WRITE_READ;
		data.status = 0x00;
		data.address = board_address;
		data.length = 0x01;
		data.data[0] = REG_STATUS; 
		data.data[1] = 0x00;
		data.data[2] = 0x00;
		data.data[3] = 0x00;
		retval = write(fd, &data, sizeof(data));
		if (retval < 0) {
			printf("Error writing get status command\n");
			return -1;
		}
		retval = read(fd, &data, sizeof(data));
		if (retval < 0) {
			printf("Error reading get status command\n");
			return -1;
		}
		if (data.status != 1) {
			printf("Error reading status\n");
			return -1;
		}
		if (data.data[0] == 0)
			break;
		if (counter > 20) {
			printf("Timeout waiting for completion\n");
			return -1;
		}
		counter++;
		printf("counter %d\n", counter);
	}

	DELAY;

	/* read 1 */
	data.type = XFER_TYPE_WRITE_READ;
	data.status = 0x00;
	data.address = board_address;
	data.length = 0x02;
	data.data[0] = REG_DATA; 
	data.data[1] = 0x00;
	data.data[2] = 0x00;
	data.data[3] = 0x00;
	retval = write(fd, &data, sizeof(data));
	if (retval < 0) {
		printf("Error writing get data byte 1 command\n");
		return -1;
	}
	read(fd, &data, sizeof(data));
	if (retval < 0) {
		printf("Error reading get date byte 1 command\n");
		return -1;
	}
	if (data.status != 1) {
		printf("Error reading data register 1\n");
		return -1;
	}
	value = data.data[0];
	value = value << 6;

	DELAY;

	/* read 2 */
	data.type = XFER_TYPE_WRITE_READ;
	data.status = 0x00;
	data.address = board_address;
	data.length = 0x02;
	data.data[0] = REG_DATA+1; 
	data.data[1] = 0x00;
	data.data[2] = 0x00;
	data.data[3] = 0x00;
	write(fd, &data, sizeof(data));
	if (retval < 0) {
		printf("Error writing get data byte 2 command\n");
		return -1;
	}
	read(fd, &data, sizeof(data));
	if (retval < 0) {
		printf("Error reading get data byte 2 command\n");
		return -1;
	}
	if (data.status != 1) {
		printf("Error reading data register 2\n");
		return -1;
	}
	temp = data.data[0];
	value = value | (temp >> 2);
	return value;
}

int get_humidity()
{
	int counter = 0;
	unsigned short value = 0;
	unsigned short temp = 0;
	int retval = 0;

	struct transfer_req data;

	/* clear status */
	data.type = XFER_TYPE_WRITE;
	data.status = 0x00;
	data.address = board_address;
	data.length = 0x02;
	data.data[0] = REG_CFG1; 
	data.data[1] = 0x00;
	data.data[2] = 0x00;
	data.data[3] = 0x00;
	write(fd, &data, sizeof(data));
	if (retval < 0) {
		printf("Error writing clear status command\n");
		return -1;
	}
	read(fd, &data, sizeof(data));
	if (retval < 0) {
		printf("Error reading clear status command\n");
		return -1;
	}
	if (data.status != 1) {
		printf("Error clearing status\n");
		return -1;
	}

	DELAY;

	/* write humidity */
	data.type = XFER_TYPE_WRITE;
	data.status = 0x00;
	data.address = board_address;
	data.length = 0x02;
	data.data[0] = REG_CFG1; 
	if (fast_conv == 0)
		data.data[1] = 0x01;
	else
		data.data[1] = 0x20 | 0x01;
	data.data[2] = 0x00;
	data.data[3] = 0x00;
	retval = write(fd, &data, sizeof(data));
	if (retval < 0) {
		printf("Error writing get humidity command\n");
		return -1;
	}
	retval = read(fd, &data, sizeof(data));
	if (retval < 0) {
		printf("Error writing get humidity command\n");
		return -1;
	}
	if (data.status != 1) {
		printf("Error writing humidity command\n");
		return -1;
	}

	DELAY;

	/* check status */
	counter = 0;
	while (1) {
		data.type = XFER_TYPE_WRITE_READ;
		data.status = 0x00;
		data.address = board_address;
		data.length = 0x01;
		data.data[0] = REG_STATUS; 
		data.data[1] = 0x00;
		data.data[2] = 0x00;
		data.data[3] = 0x00;
		retval = write(fd, &data, sizeof(data));
		if (retval < 0) {
			printf("Error writing get status command\n");
			return -1;
		}
		retval = read(fd, &data, sizeof(data));
		if (retval < 0) {
			printf("Error reading get status command\n");
			return -1;
		}
		if (data.status != 1) {
			printf("Error reading status\n");
			return -1;
		}
		if (data.data[0] == 0)
			break;
		if (counter > 20) {
			printf("Timeout waiting for completion\n");
			return -1;
		}
		counter++;
	}

	DELAY;

	/* read 1 */
	data.type = XFER_TYPE_WRITE_READ;
	data.status = 0x00;
	data.address = board_address;
	data.length = 0x02;
	data.data[0] = REG_DATA; 
	data.data[1] = 0x00;
	data.data[2] = 0x00;
	data.data[3] = 0x00;
	retval = write(fd, &data, sizeof(data));
	if (retval < 0) {
		printf("Error writing get data register 1 command\n");
		return -1;
	}
	retval = read(fd, &data, sizeof(data));
	if (retval < 0) {
		printf("Error reading get data register 1 command\n");
		return -1;
	}
	if (data.status != 1) {
		printf("Error reading data register 1\n");
		return -1;
	}
	value = data.data[0];
	value = value << 4;

	DELAY;

	/* read 2 */
	data.type = XFER_TYPE_WRITE_READ;
	data.status = 0x00;
	data.address = board_address;
	data.length = 0x02;
	data.data[0] = REG_DATA+1; 
	data.data[1] = 0x00;
	data.data[2] = 0x00;
	data.data[3] = 0x00;
	retval = write(fd, &data, sizeof(data));
	if (retval < 0) {
		printf("Error writing get data register 2 command\n");
		return -1;
	}
	retval = read(fd, &data, sizeof(data));
	if (retval < 0) {
		printf("Error reading get data register 2 command\n");
		return -1;
	}
	if (data.status != 1) {
		printf("Error reading data register 2\n");
		return -1;
	}
	temp = data.data[0];
	value = value | (temp >> 4);
	return value;
}

/*
 * @status - 1 to enable heater
 * Enable or disable heater by setting bit 3
 * in the Config2 register to 1
 */
int heater(int status)
{
	int retval = 0;
	struct transfer_req data;

	data.type = XFER_TYPE_WRITE;
	data.status = 0x00;
	data.address = board_address;
	data.length = 0x02;
	data.data[0] = REG_CFG2; 
	if (status == 0)
		data.data[1] = 0x08;
	else
		data.data[1] = 0x00;
	data.data[2] = 0x00;
	data.data[3] = 0x00;
	retval = write(fd, &data, sizeof(data));
	if (retval < 0) {
		printf("Error writing heater command\n");
		return -1;
	}
	retval = read(fd, &data, sizeof(data));
	if (retval < 0) {
		printf("Error reading heater command\n");
		return -1;
	}
	if (data.status != 1) {
		printf("Error setting heater\n");
		return -1;
	}
	return 0;
}

/*
 * @status - 1 to enable fast conversion
 * Enable or disable fast conversion by setting bit 5
 * in the Config1 register to 1
 */
void fast_conversion(int status)
{
	if (status == 0)
		fast_conv = 0;
	else
		fast_conv = 1;
}

/*
 * Get the device id of the I2C sensor connected
 */
unsigned char get_device_id()
{
	unsigned char value = 0;
	int retval = 0;

	struct transfer_req data;

	/* get I2C sensor device ID  */
	data.type = XFER_TYPE_WRITE_READ;
	data.status = 0x00;
	data.address = board_address;
	data.length = 0x01;
	data.data[0] = REG_DEVICE_ID; 
	data.data[2] = 0x00;
	data.data[3] = 0x00;
	retval = write(fd, &data, sizeof(data));
	if (retval < 0) {
		printf("Error writing get device id command\n");
		return -1;
	}
	retval = read(fd, &data, sizeof(data));
	if (retval < 0) {
		printf("Error reading get device id command\n");
		return -1;
	}
	if (data.status != 1) {
		printf("Error getting I2C sensor device ID\n");
		return -1;
	}

	value = data.data[0];
	value = value >> 4;
	return value;
}
