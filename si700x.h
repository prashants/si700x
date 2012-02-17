#ifndef _SI700X_H
#define _SI700X_H

/* IOCTL definitions */

#define SI700X_IOC_MAGIC 'k'
#define SI700X_IOC_MAXNR 9

#define SI700X_LED_ON		_IO(SI700X_IOC_MAGIC, 1)
#define SI700X_LED_OFF		_IO(SI700X_IOC_MAGIC, 2)
#define SI700X_VERSION		_IOR(SI700X_IOC_MAGIC, 3, int)
#define SI700X_PORT_COUNT	_IOR(SI700X_IOC_MAGIC, 4, int)
#define SI700X_BOARDID		_IOR(SI700X_IOC_MAGIC, 5, int)
#define SI700X_SETPROG_ON	_IO(SI700X_IOC_MAGIC, 6)
#define SI700X_SETPROG_OFF	_IO(SI700X_IOC_MAGIC, 7)
#define SI700X_SETSLEEP_ON	_IOW(SI700X_IOC_MAGIC, 8, unsigned int)
#define SI700X_SETSLEEP_OFF	_IOW(SI700X_IOC_MAGIC, 9, unsigned int)

#define XFER_TYPE_WRITE          0x10
#define XFER_TYPE_READ           0x20
#define XFER_TYPE_WRITE_READ    (XFER_TYPE_WRITE|XFER_TYPE_READ)

/* Slave Address */
#define SLAVE_NEW          0x40
#define SLAVE_LEGACY       0X20

/* Registers */
#define REG_STATUS         0x00
#define REG_DATA           0x01
#define REG_CFG1           0x03
#define REG_CFG2           0x04
#define REG_DEVICE_ID      0x11
#define REG_RAW_DATA       0x29

/* Status Register */
#define STATUS_NOT_READY   0x01

/* Config 1 Register */
#define CFG1_START_CONV    0x01
#define CFG1_HUMIDITY      0x04
#define CFG1_TEMPERATURE   0x10
#define CFG1_FAST_CONV     0x20

/* Config 2 Register */
#define CFG2_EN_TEST_REG   0x80

/* Coefficients */
#define TEMPERATURE_OFFSET 50
#define HUMIDITY_OFFSET    16
#define SLOPE              32

/* Return codes */
#define SUCCESS             0      /* Success                      */
#define ERROR_LENGTH_BAD   -1      /* DataLength is not valid      */
#define ERROR_INIT_FAIL    -2      /* Can not initialize slave     */
#define ERROR_READ_FAIL    -3      /* Can not read from slave      */
#define ERROR_WRITE_FAIL   -4      /* Can not write to slave       */
#define ERROR_TIME_OUT     -5      /* Timed out waiting for ready  */

/* Transfer status */
#define XFER_STATUS_NONE         0x00
#define XFER_STATUS_SUCCESS      0x01
#define XFER_STATUS_ADDR_NAK     0x02
#define XFER_STATUS_DATA_NAK     0x03
#define XFER_STATUS_TIMEOUT      0x04
#define XFER_STATUS_ARBLOST      0x05
#define XFER_STATUS_BAD_LENGTH   0x06
#define XFER_STATUS_BAD_MODE     0x07
#define XFER_STATUS_BAD_STATE    0x09

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

/* Maximum data bytes that can be transfered with ReadData() and WriteData() */
#define MAX_DATA_LENGTH  3

/* Maximum number of slaves on a Si7001 board */
#define MAX_SLAVE_COUNT  8

/* Maximum number of bytes in a packet */
#define MAX_PACKET_SIZE  64

/* Maximum number of I2C data bytes that can be read or written */
#define MAX_XFER_LENGTH  4

/* Maximum number of transfer requests in a packet */
#define MAX_XFER_COUNT   (MAX_PACKET_SIZE/(4+MAX_XFER_LENGTH))

/* Usb transfer request */
struct transfer_req {
	u8 type;
	u8 status;
	u8 address;
	u8 length;
	u8 data[4];
} __attribute__ ((__packed__));

#endif
