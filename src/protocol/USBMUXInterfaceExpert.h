//
// Created by Rick Mark on 11/16/20.
//

#ifndef demuxusb_USBMUXINTERFACEEXPERT_H
#define demuxusb_USBMUXINTERFACEEXPERT_H

#include "../demuxusb.hpp"
#include "InterfaceExpert.h"
#include "MuxStream.h"
#include "../ext/usbmux.h"

class USBMUXInterfaceExpert : public InterfaceExpert {
public:


private:
    std::map<uint64_t, std::shared_ptr<MuxStream>> m_streams;
};


#endif //demuxusb_USBMUXINTERFACEEXPERT_H
