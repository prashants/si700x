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

	sleep(3);

	if (ioctl(fd, SI700X_LED_OFF) == -1) {
		printf("Failed to OFF the LED: %s\n", strerror(errno));
	} else {
		printf("LED is OFF\n");
	}
	close(fd);
	return 0;
}
