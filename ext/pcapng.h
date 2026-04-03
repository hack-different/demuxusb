
#ifndef _PCAPNG_H
#define _PCAPNG_H

#include <memory>

#define TYPE_SECTION_HEADER 0x0A0D0D0A
#define TIME_INTERFACE_DESCRIPTOR_BLOCK 0x00000001
#define TYPE_ENHANCED_PACKET_BLOCK 0x06

#define USB_CONTROL_STAGE_SETUP    0x00
#define USB_CONTROL_STAGE_DATA     0x01
#define USB_CONTROL_STAGE_STATUS   0x02
#define USB_CONTROL_STAGE_COMPLETE 0x03

#define USB_EP_CONTROL     0x00
#define USB_EP_ISOCHRONOUS 0x01
#define USB_EP_BULK        0x02
#define USB_EP_INTERRUPT   0x03


#define LINKTYPE_USB_DARWIN 266
#define LINKTYPE_USBPCAP 249

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
    uint64_t uniqueId;
    uint32_t locationId;
    uint8_t speed;
    uint8_t deviceId;
    uint8_t endpointId;
    uint8_t endpointType;
} darwin_usb_header_1_0_t;

typedef struct {
    uint16_t version;
    uint8_t headerLength;
    uint8_t type;
    uint32_t length;
    uint32_t status;
    uint32_t isoFrames;
    uint64_t uniqueId;
    uint32_t locationId;
    uint8_t speed;
    uint8_t deviceId;
    uint8_t endpointId;
    uint8_t endpointType;
    uint8_t usbClass;
    uint8_t usbSubclass;
    uint8_t usbInterface;
    uint8_t usbInterfaceAlternate;
    uint16_t usbVendorId;
    uint16_t usbProductId;
} darwin_usb_header_1_1_t;


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
} usb_setup_t;

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
    usb_setup_t setup;
} pcap_usb_header_t;

#pragma pack(1)
typedef struct
{
    uint16_t       headerLen; /* This header length */
    uint64_t       irpId;     /* I/O Request packet ID */
    uint32_t  status;    /* USB status code
                               (on return from host controller) */
    uint16_t       function;  /* URB Function */
    uint8_t        info;      /* I/O Request info */

    uint16_t       bus;       /* bus (RootHub) number */
    uint16_t       device;    /* device address */
    uint8_t        endpoint;  /* endpoint number and transfer direction */
    uint8_t        transfer;  /* transfer type */

    uint32_t       dataLength;/* Data length */
} usbpcap_packet_header_t;

#define USBPCAP_TRANSFER_ISOCHRONOUS 0
#define USBPCAP_TRANSFER_INTERRUPT 1
#define USBPCAP_TRANSFER_CONTROL 2
#define USBPCAP_TRANSFER_BULK 3

#define USBPCAP_CONTROL_STAGE_SETUP    0
#define USBPCAP_CONTROL_STAGE_DATA     1
#define USBPCAP_CONTROL_STAGE_STATUS   2
#define USBPCAP_CONTROL_STAGE_COMPLETE 3

#pragma pack(1)
typedef struct
{
    usbpcap_packet_header_t  header;
    uint8_t                         stage;
} usbpcap_buffer_control_header_t;

typedef struct {
    uint16_t linktype;    // Link layer type (e.g., Ethernet, WLAN)
    uint16_t reserved;    // Not used; must be 0
    uint32_t snaplen;     // Max number of octets captured from each packet
    /* Options follow here (variable length, padded to 32 bits) */
} pcap_interface_descriptor_t;

#endif