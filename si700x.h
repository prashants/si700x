#ifndef _SI700X_H
#define _SI700X_H

/* IOCTL definitions */

#define SI700X_IOC_MAGIC 'k'
#define SI700X_IOC_MAXNR 5

#define SI700X_LED_ON      _IO(SI700X_IOC_MAGIC,  1)
#define SI700X_LED_OFF     _IO(SI700X_IOC_MAGIC,  2)
#define SI700X_VERSION     _IOR(SI700X_IOC_MAGIC,  3, int)
#define SI700X_PORT_COUNT  _IOR(SI700X_IOC_MAGIC,  4, int)
#define SI700X_BOARDID     _IOR(SI700X_IOC_MAGIC,  5, int)

#define XFER_TYPE_WRITE          0x10
#define XFER_TYPE_READ           0x20
#define XFER_TYPE_WRITE_READ    (XFER_TYPE_WRITE|XFER_TYPE_READ)

#endif
