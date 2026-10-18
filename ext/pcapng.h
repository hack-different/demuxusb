#ifndef _PCAPNG_H
#define _PCAPNG_H

#include <cstdint>

/* PCAPNG block types */
#define TYPE_SECTION_HEADER 0x0A0D0D0A
#define TIME_INTERFACE_DESCRIPTOR_BLOCK 0x00000001
#define TYPE_SIMPLE_PACKET_BLOCK 0x00000003
#define TYPE_ENHANCED_PACKET_BLOCK 0x06

#define PCAPNG_BYTE_ORDER_MAGIC 0x1A2B3C4D
#define PCAPNG_BYTE_ORDER_MAGIC_SWAPPED 0x4D3C2B1A

/* Classic libpcap file magic numbers (as read in host byte order) */
#define PCAP_MAGIC_MICROSECONDS 0xA1B2C3D4
#define PCAP_MAGIC_NANOSECONDS 0xA1B23C4D
#define PCAP_MAGIC_MICROSECONDS_SWAPPED 0xD4C3B2A1
#define PCAP_MAGIC_NANOSECONDS_SWAPPED 0x4D3CB2A1

#define USB_CONTROL_STAGE_SETUP    0x00
#define USB_CONTROL_STAGE_DATA     0x01
#define USB_CONTROL_STAGE_STATUS   0x02
#define USB_CONTROL_STAGE_COMPLETE 0x03

#define USB_EP_CONTROL     0x00
#define USB_EP_ISOCHRONOUS 0x01
#define USB_EP_BULK        0x02
#define USB_EP_INTERRUPT   0x03


#define LINKTYPE_USB_LINUX 189
#define LINKTYPE_USB_LINUX_MMAPPED 220
#define LINKTYPE_USBPCAP 249
#define LINKTYPE_USB_DARWIN 266

#define DARWIN_IO_SUBMIT   0
#define DARWIN_IO_COMPLETE 1

#define DARWIN_USB_HEADER_VERSION_1_0 0x0100
#define DARWIN_USB_HEADER_VERSION_1_1 0x0101


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
    uint32_t byte_order_magic;
    uint16_t major_version;
    uint16_t minor_version;
    int64_t section_length;
    /* ... Options ... */
} pcapng_shb_t;

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
    uint32_t packet_len;
    /* ... Packet Data ... */
} pcapng_spb_t;

typedef struct {
    uint32_t magic;
    uint16_t version_major;
    uint16_t version_minor;
    int32_t thiszone;
    uint32_t sigfigs;
    uint32_t snaplen;
    uint32_t linktype;
} pcap_file_header_t;

typedef struct {
    uint32_t ts_sec;
    uint32_t ts_usec;
    uint32_t incl_len;
    uint32_t orig_len;
} pcap_record_header_t;

typedef struct {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} usb_setup_t;

/* Linux usbmon header (LINKTYPE_USB_LINUX), LINKTYPE_USB_LINUX_MMAPPED appends pcap_usb_header_mmapped_t */
#define USBMON_EVENT_SUBMIT 'S'
#define USBMON_EVENT_COMPLETE 'C'
#define USBMON_EVENT_ERROR 'E'

#define USBMON_TRANSFER_ISOCHRONOUS 0
#define USBMON_TRANSFER_INTERRUPT 1
#define USBMON_TRANSFER_CONTROL 2
#define USBMON_TRANSFER_BULK 3

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

typedef struct {
    pcap_usb_header_t header;
    int32_t interval;
    int32_t start_frame;
    uint32_t xfer_flags;
    uint32_t ndesc;
} pcap_usb_header_mmapped_t;

#define USBMON_ISO_DESCRIPTOR_SIZE 16

#pragma pack(push, 1)
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

#define USBPCAP_INFO_PDO_TO_FDO 0x01

#define USBPCAP_TRANSFER_ISOCHRONOUS 0
#define USBPCAP_TRANSFER_INTERRUPT 1
#define USBPCAP_TRANSFER_CONTROL 2
#define USBPCAP_TRANSFER_BULK 3

#define USBPCAP_CONTROL_STAGE_SETUP    0
#define USBPCAP_CONTROL_STAGE_DATA     1
#define USBPCAP_CONTROL_STAGE_STATUS   2
#define USBPCAP_CONTROL_STAGE_COMPLETE 3

typedef struct
{
    usbpcap_packet_header_t  header;
    uint8_t                         stage;
} usbpcap_buffer_control_header_t;
#pragma pack(pop)

typedef struct {
    uint16_t linktype;    // Link layer type (e.g., Ethernet, WLAN)
    uint16_t reserved;    // Not used; must be 0
    uint32_t snaplen;     // Max number of octets captured from each packet
    /* Options follow here (variable length, padded to 32 bits) */
} pcap_interface_descriptor_t;

static_assert(sizeof(darwin_usb_header_1_0_t) == 32, "Darwin 1.0 header must be 32 bytes");
static_assert(sizeof(pcap_usb_header_t) == 48, "usbmon header must be 48 bytes");
static_assert(sizeof(pcap_usb_header_mmapped_t) == 64, "usbmon mmapped header must be 64 bytes");
static_assert(sizeof(usbpcap_packet_header_t) == 27, "USBPcap header must be 27 bytes");

#endif
