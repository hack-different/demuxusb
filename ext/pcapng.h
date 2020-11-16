
#ifndef _PCAPNG_H
#define _PCAPNG_H

#include <memory>

#define TYPE_EPB 0x06

#define USB_CONTROL_STAGE_SETUP    0x00
#define USB_CONTROL_STAGE_DATA     0x01
#define USB_CONTROL_STAGE_STATUS   0x02
#define USB_CONTROL_STAGE_COMPLETE 0x03

#define USB_EP_CONTROL     0x00
#define USB_EP_ISOCHRONOUS 0x01
#define USB_EP_BULK        0x02
#define USB_EP_INTERRUPT   0x03

#define DARWIN_IO_SUBMIT   0
#define DARWIN_IO_COMPLETE 1


/* USB speeds */
#define DARWIN_SPEED_LOW         0
#define DARWIN_SPEED_FULL        1
#define DARWIN_SPEED_HIGH        2
#define DARWIN_SPEED_SUPER       3
#define DARWIN_SPEED_SUPERPLUS   4


typedef struct {
    uint16_t version;
    uint8_t headerLength;
    uint8_t type;
    uint32_t length;
    uint32_t status;
    uint32_t isoFrames;
    uint32_t uniqueId;
    uint32_t locationId;
    uint8_t speed;
    uint8_t deviceId;
    uint8_t endpointId;
    uint8_t endpointType;
} darwin_usb_header_t;



typedef struct {
    uint32_t type;
    uint32_t length;
} block_t;

typedef struct {
    uint32_t interface_id;
    uint32_t timestamp_high;
    uint32_t timestamp_low;
    uint32_t captured_len;
    uint32_t packet_len;
    /* ... Packet Data ... */
    /* ... Padding ... */
    /* ... Options ... */
} pcapng_epb_t;

typedef struct {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} pcap_usb_setup_t;

typedef struct {
    uint64_t id;
    uint8_t event_type;
    uint8_t transfer_type;
    uint8_t endpoint_number;
    uint8_t device_address;
    uint16_t bus_id;
    char setup_flag;/*if !=0 the urb setup header is not present*/
    char data_flag; /*if !=0 no urb data is present*/
    int64_t ts_sec;
    int32_t ts_usec;
    int32_t status;
    uint32_t urb_len;
    uint32_t data_len; /* amount of urb data really present in this event*/
    pcap_usb_setup_t setup;
} pcap_usb_header_t;

#endif