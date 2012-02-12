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

int fd;

int get_temperature(void);
int get_humidity(void);

int main()
{
	short int version = 0;
	char port_count;
	char board_id;
	unsigned int port_id;

	int temperature;
	int humidity;

	fd = open("/dev/si700x0", O_RDWR);
	if (fd < 0) {
		printf("cannot open device file\n");
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

	/* read temperature */
	temperature = get_temperature();
	if (temperature < 0) {
		printf("Error reading temperature\n");
	} else {
		printf("Current temperature is : %f\n", (((float)temperature / 32.0) - 50.0));
	}

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

int get_temperature()
{
	int size = 0;
	int counter = 0;
	unsigned short value = 0;
	unsigned short temp = 0;

	struct transfer_req data;

	/* clear status */
	data.type = XFER_TYPE_WRITE;
	data.status = 0x00;
	data.address = 0x42;
	data.length = 0x02;
	data.data[0] = REG_CFG1; 
	data.data[1] = 0x00;
	data.data[2] = 0x00;
	data.data[3] = 0x00;
	size = write(fd, &data, sizeof(data));
	size = read(fd, &data, sizeof(data));
	if (data.status != 1) {
		printf("Error clearing status\n");
		return -1;
	}

	/* write temperature */
	data.type = XFER_TYPE_WRITE;
	data.status = 0x00;
	data.address = 0x42;
	data.length = 0x02;
	data.data[0] = REG_CFG1; 
	data.data[1] = 0x11;
	data.data[2] = 0x00;
	data.data[3] = 0x00;
	size = write(fd, &data, sizeof(data));
	size = read(fd, &data, sizeof(data));
	if (data.status != 1) {
		printf("Error writing temparature command\n");
		return -1;
	}

	sleep(1);

	/* check status */
	while (1) {
		data.type = XFER_TYPE_WRITE_READ;
		data.status = 0x00;
		data.address = 0x42;
		data.length = 0x01;
		data.data[0] = REG_STATUS; 
		data.data[1] = 0x00;
		data.data[2] = 0x00;
		data.data[3] = 0x00;
		size = write(fd, &data, sizeof(data));
		size = read(fd, &data, sizeof(data));
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

	/* read 1 */
	data.type = XFER_TYPE_WRITE_READ;
	data.status = 0x00;
	data.address = 0x42;
	data.length = 0x02;
	data.data[0] = REG_DATA; 
	data.data[1] = 0x00;
	data.data[2] = 0x00;
	data.data[3] = 0x00;
	size = write(fd, &data, sizeof(data));
	size = read(fd, &data, sizeof(data));
	if (data.status != 1) {
		printf("Error reading data register 1\n");
		return -1;
	}
	value = data.data[0];
	value = value << 6;

	/* read 2 */
	data.type = XFER_TYPE_WRITE_READ;
	data.status = 0x00;
	data.address = 0x42;
	data.length = 0x02;
	data.data[0] = REG_DATA+1; 
	data.data[1] = 0x00;
	data.data[2] = 0x00;
	data.data[3] = 0x00;
	size = write(fd, &data, sizeof(data));
	size = read(fd, &data, sizeof(data));
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
	int size = 0;
	int counter = 0;
	unsigned short value = 0;
	unsigned short temp = 0;

	struct transfer_req data;

	/* clear status */
	data.type = XFER_TYPE_WRITE;
	data.status = 0x00;
	data.address = 0x42;
	data.length = 0x02;
	data.data[0] = REG_CFG1; 
	data.data[1] = 0x00;
	data.data[2] = 0x00;
	data.data[3] = 0x00;
	size = write(fd, &data, sizeof(data));
	size = read(fd, &data, sizeof(data));
	if (data.status != 1) {
		printf("Error clearing status\n");
		return -1;
	}

	/* write humidity */
	data.type = XFER_TYPE_WRITE;
	data.status = 0x00;
	data.address = 0x42;
	data.length = 0x02;
	data.data[0] = REG_CFG1; 
	data.data[1] = 0x01;
	data.data[2] = 0x00;
	data.data[3] = 0x00;
	size = write(fd, &data, sizeof(data));
	size = read(fd, &data, sizeof(data));
	if (data.status != 1) {
		printf("Error writing humidity command\n");
		return -1;
	}

	sleep(1);

	/* check status */
	while (1) {
		data.type = XFER_TYPE_WRITE_READ;
		data.status = 0x00;
		data.address = 0x42;
		data.length = 0x01;
		data.data[0] = REG_STATUS; 
		data.data[1] = 0x00;
		data.data[2] = 0x00;
		data.data[3] = 0x00;
		size = write(fd, &data, sizeof(data));
		size = read(fd, &data, sizeof(data));
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

	/* read 1 */
	data.type = XFER_TYPE_WRITE_READ;
	data.status = 0x00;
	data.address = 0x42;
	data.length = 0x02;
	data.data[0] = REG_DATA; 
	data.data[1] = 0x00;
	data.data[2] = 0x00;
	data.data[3] = 0x00;
	size = write(fd, &data, sizeof(data));
	size = read(fd, &data, sizeof(data));
	if (data.status != 1) {
		printf("Error reading data register 1\n");
		return -1;
	}
	value = data.data[0];
	value = value << 4;

	/* read 2 */
	data.type = XFER_TYPE_WRITE_READ;
	data.status = 0x00;
	data.address = 0x42;
	data.length = 0x02;
	data.data[0] = REG_DATA+1; 
	data.data[1] = 0x00;
	data.data[2] = 0x00;
	data.data[3] = 0x00;
	size = write(fd, &data, sizeof(data));
	size = read(fd, &data, sizeof(data));
	if (data.status != 1) {
		printf("Error reading data register 2\n");
		return -1;
	}
	temp = data.data[0];
	value = value | (temp >> 4);
	return value;
}
