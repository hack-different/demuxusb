//
// Created by Rick Mark on 11/16/20.
//

#ifndef DEUSBMUX_USBMUXINTERFACEEXPERT_H
#define DEUSBMUX_USBMUXINTERFACEEXPERT_H

#include "deusbmux.hpp"
#include "InterfaceExpert.h"
#include "MuxStream.h"

class USBMUXInterfaceExpert : public InterfaceExpert {
public:


private:
    std::map<uint8_t, std::shared_ptr<MuxStream>> m_streams;
};


#endif //DEUSBMUX_USBMUXINTERFACEEXPERT_H
