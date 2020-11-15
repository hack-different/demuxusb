
#ifndef _PCAPNG_H
#define _PCAPNG_H

#include <memory>

#define TYPE_EPB 0x06

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