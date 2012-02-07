#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "si700x.h"

int main()
{
	int fd;
	short int version = 0;
	char port_count;
	char board_id;
	char port_id;

	char buf = 'A';

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

	write(fd, &buf, 1);
	printf("data %c\n", buf);

	if (ioctl(fd, SI700X_LED_OFF) == -1) {
		printf("Failed to OFF the LED: %s\n", strerror(errno));
	} else {
		printf("LED is OFF\n");
	}
	close(fd);
	return 0;
}
