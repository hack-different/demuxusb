//
// Created by Rick Mark on 11/16/20.
//

#ifndef demuxusb_DFUINTERFACEEXPERT_H
#define demuxusb_DFUINTERFACEEXPERT_H

#include "../demuxusb.hpp"
#include "InterfaceExpert.h"

class DFUInterfaceExpert : public InterfaceExpert {
public:
    DFUInterfaceExpert(uint8_t configuration, uint8_t interface) : InterfaceExpert(configuration, interface) {};

    std::string toString() override;

protected:

};


#endif //demuxusb_DFUINTERFACEEXPERT_H
