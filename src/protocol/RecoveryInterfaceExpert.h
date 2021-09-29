//
// Created by Rick Mark on 11/16/20.
//

#ifndef demuxusb_RECOVERYINTERFACEEXPERT_H
#define demuxusb_RECOVERYINTERFACEEXPERT_H

#include "../demuxusb.hpp"
#include "InterfaceExpert.h"

struct RecoveryPayload {

};

class RecoveryInterfaceExpert : public InterfaceExpert {
public:
    RecoveryInterfaceExpert(uint8_t configuration, uint8_t interface) : InterfaceExpert(configuration, interface) {};
    std::string toString() override;
    bool isEmpty() override;
protected:
    std::vector<RecoveryPayload> m_payloads{};
};


#endif //demuxusb_RECOVERYINTERFACEEXPERT_H
