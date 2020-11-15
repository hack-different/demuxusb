//
// Created by Rick Mark on 11/14/20.
//

#ifndef DEUSBMUX_CDEVICE_H
#define DEUSBMUX_CDEVICE_H

#include <pcapng.h>
#include <usb.h>

class CDevice {
public:
    explicit CDevice(uint32_t id);

    void processPacket(pcap_usb_header_t* header, void* data);
protected:
    uint32_t m_device;
};


#endif //DEUSBMUX_CDEVICE_H
