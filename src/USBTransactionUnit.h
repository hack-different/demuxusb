//
// Created by Rick Mark on 2/28/26.
//

#ifndef DEMUXUSB_USBTRANSACTIONUNIT_H
#define DEMUXUSB_USBTRANSACTIONUNIT_H

typedef enum {
    USB_DIRECTION_IN,
    USB_DIRECTION_OUT
} urb_direction_t;

typedef enum {
    USB_SPEED_HIGH,
    USB_SPEED_FULL,
    USB_SPEED_SUPER
} urb_speed_t;

class USBTransactionUnit {
public:
    explicit USBTransactionUnit(urb_direction_t direction, urb_speed_t speed, uint8_t device_id, uint8_t endpoint_id,
        uint64_t index, std::vector<std::byte> data_buffer);
    virtual ~USBTransactionUnit();

    virtual std::vector<std::byte> getData();
    virtual std::size_t getDataSize();


private:
    urb_direction_t direction;
    urb_speed_t speed;
    std::vector<std::byte> data_buffer;
    uint64_t index;
    double_t relative_time;
    double_t duration;
};


#endif //DEMUXUSB_USBTRANSACTIONUNIT_H