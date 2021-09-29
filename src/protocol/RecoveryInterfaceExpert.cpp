//
// Created by Rick Mark on 11/16/20.
//

#include "RecoveryInterfaceExpert.h"

bool RecoveryInterfaceExpert::isEmpty() {
    return this->m_payloads.empty();
}

std::string RecoveryInterfaceExpert::toString() {
    return InterfaceExpert::toString();
}
