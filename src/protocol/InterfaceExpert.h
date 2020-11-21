//
// Created by Rick Mark on 11/16/20.
//

#ifndef demuxusb_INTERFACEEXPERT_H
#define demuxusb_INTERFACEEXPERT_H

#include "../demuxusb.hpp"

class InterfaceExpert {
public:
    virtual ~InterfaceExpert() = default;
    virtual void processControlOut(uint8_t interfaceId, byte_array data) {};
    virtual void processControlIn(uint8_t interfaceId, byte_array data) {};
    virtual void processBulkOut(uint8_t interfaceId, byte_array data) {};
    virtual void processBulkIn(uint8_t interfaceId, byte_array data) {};
};


#endif //demuxusb_INTERFACEEXPERT_H
