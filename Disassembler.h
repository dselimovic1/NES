//
// Created by denis on 02/03/2020.
//

#ifndef NES_DISASSEMBLER_H
#define NES_DISASSEMBLER_H


#include <string>
#include <map>
#include "Instruction.h"

class Disassembler {

    std::string toHex(uint32_t number, uint8_t length);
    std::string getInstruction(const std::string &instruction, uint32_t operand);
    void addInstruction(const uint16_t &address, const std::string &instruction);

    std::map<uint16_t, std::string> disassembly;
public:
    Disassembler() = default;
    ~Disassembler() = default;
};


#endif //NES_DISASSEMBLER_H
