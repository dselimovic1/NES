//
// Created by denis on 24/02/2020.
//

#ifndef NES_GAMEPAK_H
#define NES_GAMEPAK_H

#include <string>
#include <vector>
#include "Mapper.h"

class GamePak {
public:
    enum MIRRORING {HORIZONTAL, VERTICAL} mirroring = HORIZONTAL;

private:
    //GamePak se sastoji PRG (programske memorije PRG-ROM) i CHR(memorija sprite-ova CHR-ROM)
    //njih učitamo iz .nes datoteke
    std::vector<uint8_t> PRG_ROM;
    std::vector<uint8_t> CHR_ROM;

    //pomoćna struktura za učitavanje header-a .nes fajla
    //na osnovu ovoga saznajemo informacije o vrsti .nes fajla
    //možemo odrediti kapacitet PRG i CHR memorije
    struct NESHeader {
        uint8_t header[4];
        uint8_t prg_rom_size;
        uint8_t chr_rom_size;
        uint8_t flags_mirroring_trainer;
        uint8_t flags_playchoice;
        uint8_t flags_prg_ram_size;
        uint8_t tv_system;
        uint8_t prg_ram_presence;
        uint8_t unused[5];
    };

    //pokazivač na mapper koji se koristi za mapiranje adresa
    Mapper *mapper = nullptr;
    //funkcija za određivanje koji mapper se koristi
    static Mapper *setMapper(uint8_t mapperID, uint8_t prg_banks, uint8_t chr_banks);

public:
    explicit GamePak(const std::string &game);
    ~GamePak();
    bool readCPUMemory(uint16_t address, uint8_t &data);
    bool writeCPUMemory(uint16_t address, uint8_t data);
    bool readPPUMemory(uint16_t address, uint8_t &data);
    bool writePPUMemory(uint16_t address, uint8_t data);
    void reset();
};


#endif //NES_GAMEPAK_H
