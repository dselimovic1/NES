//
// Created by denis on 24/02/2020.
//

#include "ppu2C02.h"

uint8_t ppu2C02::readCPUMemory(uint16_t address) {
    uint8_t data = 0x00;
    switch(address) {
        case 0x0000:
        case 0x0001:
            //PPUCTRL i PPUMASK su write-only registri
            break;
        case 0x0002:
            //čitanje iz ovog registra utiče i na ostale registre
            //postavlja se toggle na 0
            //takođe vblank se postavlja na 0

            //samo najznačanija tri bita su validna
            data = ppustatus.reg & 0xE0u;
            toggle = false;
            ppustatus.vblank = 0;
            break;
        case 0x0003:
            //OAM_ADDRESS nije readable
            break;
        case 0x0004:
            //pristup oam memoriji je moguć uvijek
            data = oam_memory[address];
            break;
        case 0x0005:
        case 0x0006:
            //PPUSCROLL i VRAM_ADDRESS su write-only
            break;
        case 0x0007:
            //koristimo buffer od ranije da vratimo podatke koji su pročitani u prethodnom ciklusu
            data = ppudata_buffer;
            //sada počinje sljedeći ciklus čitanja
            ppudata_buffer = readPPUMemory(vram_address.reg);

            //ako je adresa >= 0x3F00 onda se radi o čitanju paleta
            //ovo je moguće uraditi bez čekanja i u tom slučaju se podaci vraćaju odmah
            if(vram_address.reg >= 0x3F00) data = ppudata_buffer;

            //na kraju inkrementiramo adresu
            vram_address.reg += (ppuctrl.increment_mode ? 32 : 1);
            break;
        default:
            break;
    }
    return data;
}

void ppu2C02::writeCPUMemory(uint16_t address, uint8_t data) {
    switch(address) {
        case 0x0000:
            //upis u ovaj registar je moguć
            //upis u ovaj registar vrši upis i u PPUSCROLL registar - t_address pomoćna varijabla
            ppuctrl.reg = data;
            t_address.nametable_select_x = ppuctrl.nametable_select_1;
            t_address.nametable_select_y = ppuctrl.nametable_select_2;
            break;
        case 0x0001:
            //upis u mask registar je isto moguć
            ppumask.reg = data;
            break;
        case 0x0002:
            //upis u ovaj registar nije moguć
            break;
        case 0x0003:
            //upis adrese za OAM
            OAM_address = data;
            break;
        case 0x0004:
            //upis u OAM preko sadržaja četvrtog registra
            //ua upis koristimo pokazivač na tu memoriju
            oam_memory[OAM_address] = data;
            break;
        case 0x0005:
            //upis u PPUSCROLL ide u dvije faze
            //prvo vršimo upis u X scroll, pa onda u Y scroll
            //ovo se određuje na osnovu latch varijable - toggle
            if(!toggle) {
                //prvo postavimo vrijednost fine_x i coarse_x
                fine_x = data & 0x07u;
                t_address.coarse_x = data >> 3u;
                // postavimo toggle na 1;
                toggle = true;
            }
            else {
                //postavimo fine_y i coarse_y
                //vratimo toggle na 0
                t_address.fine_y = data & 0x07u;
                t_address.coarse_y = data >> 3u;
                toggle = false;
            }
            break;
        case 0x0006:
            //t_address koritimo kao buffer za vram_address
            //upis se vrši iz dva dijela
            //kad je latch u nuli upišemo high byte
            //nakon toga upišemo low byte
            if(!toggle) {
                //adresu svodimo na opseg 0x3F i generišemo high byte
                t_address.reg = (uint16_t)(((data & 0x3Fu) << 8u) | (t_address.reg & 0x00FFu));
                toggle = true;
            }
            else {
                //sada dodajemo low byte
                t_address.reg = (t_address.reg & 0xFF00u) | data;
                vram_address = t_address;
                toggle = false;
            }
            break;
        case 0x0007:
            //upis u VRAM
            //koristmo writePPUMemory funkciju
            //inkrementiramo adresu
            writePPUMemory(vram_address.reg, data);
            //adresu inkrementiramo na osnovu PPUCTRL regista za 1 ili 32
            vram_address.reg += (ppuctrl.increment_mode ? 32 : 1);
            break;
        default:
            break;
    }
}

uint8_t ppu2C02::readPPUMemory(uint16_t address) {
    //mapiramo adresu na opseg 0x0000 do 0x3fff
    uint8_t data = 0x00;
    address &= 0x3FFFu;
    if(gamepak->readPPUMemory(address, data)) {}
    else if(address >= 0x0000 && address <= 0x1FFF) return pattern_table[(address & 0x1000u) >> 12u][address & 0x0FFFu];
    else if(address >= 0x2000 && address <= 0x3EFF) {
        address &= 0xFFFu;
        //ako se koristi vertikalni mirroring prvi i treći nametable su isti, kao i drugi i četvrti
        //ako se koristi horizontalni mirroring prvi i drugi nametable su isti i mapiraju se u isti adresni prostor, kao i treći i četvrti
        if(gamepak->mirroring == GamePak::MIRRORING::VERTICAL) {
            if((address >= 0x0000 && address <= 0x03FF) || (address >= 0x0800 && address <= 0x0BFF)) return nametable[0][address & 0x03FFu];
            else if ((address >= 0x0400 && address <= 0x07FF) || (address >= 0x0C00 && address <= 0x0FFF))return nametable[1][address & 0x03FFu];
        }
        else if(gamepak->mirroring == GamePak::MIRRORING::HORIZONTAL) {
            if(address >= 0x0000 && address <= 0x07FF) return nametable[0][address & 0x03FFu];
            else if(address >= 0x0800 && address <= 0x0FFF) return nametable[1][address & 0x03FFu];
        }
    }
    else if(address >= 0x3F00 && address <= 0x3FFF) {
        //adrese od 0x3F00 do 0x3F1F se preslikavaju u ostale adrese
        //adresu svodimo na opseg od 0x0000 do 0x001F da bi pristupili nizu
        address &= 0x001Fu;
        //parovi koji se preslikavaju su 0x3f10->0x3f00, 0x3f14->0x3f04, 0x3f18->0x3f08, 0x3f1c->0x3f0c
        if (address == 0x0010) address = 0x0000;
        else if (address == 0x0014) address = 0x0004;
        else if (address == 0x0018) address = 0x0008;
        else if (address == 0x001C) address = 0x000C;
        return pallete[address] & unsigned(ppumask.grayscale ? 0x30 : 0x3F);
    }
    return data;
}

void ppu2C02::writePPUMemory(uint16_t address, uint8_t data) {
    //mapiramo adresu na opseg 0x0000 do 0x3fff
    address &= 0x3FFFu;
    if(gamepak->writePPUMemory(address, data)) {}
    else if(address >= 0x0000 && address <= 0x1FFF) pattern_table[(address & 0x1000u) >> 12u][address & 0x0FFFu] = data;
    else if(address >= 0x2000 && address <= 0x3EFF) {
        address &= 0xFFFu;
        //ako se koristi vertikalni mirroring prvi i treći nametable su isti, kao i drugi i četvrti
        //ako se koristi horizontalni mirroring prvi i drugi nametable su isti i mapiraju se u isti adresni prostor, kao i treći i četvrti
        if(gamepak->mirroring == GamePak::MIRRORING::VERTICAL) {
            if((address >= 0x0000 && address <= 0x03FF) || (address >= 0x0800 && address <= 0x0BFF)) nametable[0][address & 0x03FFu] = data;
            else if ((address >= 0x0400 && address <= 0x07FF) || (address >= 0x0C00 && address <= 0x0FFF)) nametable[1][address & 0x03FFu] = data;
        }
        else if(gamepak->mirroring == GamePak::MIRRORING::HORIZONTAL) {
            if(address >= 0x0000 && address <= 0x07FF) nametable[0][address & 0x03FFu] = data;
            else if(address >= 0x0800 && address <= 0x0FFF) nametable[1][address & 0x03FFu] = data;
        }
    }
    else if(address >= 0x3F00 && address <= 0x3FFF) {
        //adrese od 0x3F00 do 0x3F1F se preslikavaju u ostale adrese
        //adresu svodimo na opseg od 0x0000 do 0x001F da bi pristupili nizu
        address &= 0x001Fu;
        //parovi koji se preslikavaju su 0x3f10->0x3f00, 0x3f14->0x3f04, 0x3f18->0x3f08, 0x3f1c->0x3f0c
        if (address == 0x0010) address = 0x0000;
        else if (address == 0x0014) address = 0x0004;
        else if (address == 0x0018) address = 0x0008;
        else if (address == 0x001C) address = 0x000C;
        pallete[address] = data;
    }
}

void ppu2C02::connectGamePak(GamePak *gamePak){
    this->gamepak = gamePak;
}

void ppu2C02::clock() {
    if(scanline >= -1 && scanline < 240) {
        if(scanline == 0 && cycles == 0) cycles = 1;
        if(scanline == -1 && cycles == 1) ppustatus.vblank = 0;
        if((cycles >= 2 && cycles < 258) || (cycles >= 321 && cycles < 338)) {
            updateShiftRegister();
            uint8_t tile_selector = (cycles - 1) % 8;
            fetchNextTile(tile_selector);
        }
        if(cycles == 256) scrollingVertical();
        if(cycles == 257) {
            loadPixel();
            transferHorizontal();
        }
        if(cycles == 338 || cycles == 340) tile_id = readPPUMemory(0x2000u | (vram_address.reg && 0x0FFFu));
        if(scanline == -1 && cycles >= 280 && cycles < 305) transferVertical();
    }
    if(scanline >= 241 && scanline < 261) {
        if(scanline == 241 && cycles == 1) {
            ppustatus.vblank = 1;
            if(ppuctrl.nmi_enable) interrupt = true;
        }
    }

    Palette palette{};
    SpritePalette spritePalette{};
    if(ppumask.background_enable) palette = getComposition();
    if(ppumask.sprite_enable) spritePalette = getSpriteComposition();
    FinalPalette finalPalette = getFinalComposition(palette, spritePalette);

    cycles++;
    if(cycles >= 341) {
        cycles = 0;
        scanline++;
        if(scanline >= 261) scanline = -1;
    }
}

void ppu2C02::reset() {
    ppustatus.reg = 0x00;
    ppumask.reg = 0x00;
    ppuctrl.reg = 0x00;
    vram_address.reg = 0x0000;
    t_address.reg = 0x0000;
    ppudata_buffer = 0x00;
    toggle = false;
    fine_x = 0x00;
    cycles = 0;
}

void ppu2C02::scrollingHorizontal() {
    // Ako su biti za rendering pozadine ili sprite-a postavljeni može se izvršiti scroll horizontalno
    if(ppumask.background_enable || ppumask.sprite_enable) {
        // prilikom skrolanja možemo preći u sljedeći nametable
        // ukoliko se to desi moramo ažurirati bit koji označava sljedeći nametable
        // u suprotnom samo povećamo adresu sa koje se čita sljedeći bajt
        // jedan nametable ima 32x30 pločica, nakon što naiđemo na 31. pločicu u redu prelazimo na sljedeći nametable
        if(vram_address.coarse_x == 31) {
            vram_address.coarse_x = 0;
            vram_address.nametable_select_x = ~vram_address.nametable_select_x;
        }
        else {
            vram_address.coarse_x++;
        }
    }
}

void ppu2C02::scrollingVertical() {
    // scroll vertikalno je isto moguć samo kad su biti za rendering omogućeni
    if(ppumask.background_enable || ppumask.sprite_enable) {
        // scroll vertikalno ne ide na svakih 8 piksela (kolika je visina jedne pločice)
        // moguć je scroll za određeni broj piksela, zato koristimo fine_y varijablu
        // pored toga, kako nametable ima 32x32 pločica, a vidljivo je samo 32x30 posljednja dva reda traže poseban tretman
        if(vram_address.fine_y < 7) {
            vram_address.fine_y++;
        }
        else {
            vram_address.fine_y = 0;
            if(vram_address.coarse_y == 29 || vram_address.coarse_y == 31) {
                vram_address.coarse_y = 0;
                if(vram_address.coarse_y == 29) vram_address.nametable_select_y = ~vram_address.nametable_select_y;
            }
            else {
                vram_address.coarse_y++;
            }
        }
    }
}

void ppu2C02::transferHorizontal() {
    if(ppumask.sprite_enable || ppumask.background_enable) {
        vram_address.coarse_x = t_address.coarse_x;
        vram_address.nametable_select_x = t_address.nametable_select_x;
    }

}

void ppu2C02::transferVertical() {
    if(ppumask.sprite_enable || ppumask.background_enable) {
        vram_address.coarse_y = t_address.coarse_y;
        vram_address.nametable_select_y = t_address.nametable_select_y;
        vram_address.fine_y = t_address.fine_y;
    }
}

void ppu2C02::loadPixel() {
    // PPU ima 16-bitni shift registar
    // prvih 8 najznačajnijih bita predstavljaju piksel koji se crta
    // sljedeći piksel koji će se crtati u novom ciklusu je u donjih 8 bita registra
    // ova funkcija ažurira donjih 8 bita shift registra
    shifter_pattern_low = (shifter_attribute_low & 0xFF00u) | tile_lsb;
    shifter_pattern_high = (shifter_attribute_high & 0xFF00u) | tile_msb;
    shifter_attribute_low = (shifter_attribute_low & 0xFF00u) | ((tile_attribute & 0b01u) ? 0xFF : 0x00);
    shifter_attribute_high = (shifter_attribute_high & 0xFF00u) | ((tile_attribute & 0b10u) ? 0xFF : 0x00);
}

void ppu2C02::updateShiftRegister() {
    // ažurira shift registre za svaki ciklus PPU
    shifter_pattern_low <<= 1u;
    shifter_pattern_high <<= 1u;
    shifter_attribute_low <<= 1u;
    shifter_attribute_high <<= 1u;
}

void ppu2C02::fetchNextTile(uint8_t selector) {
    uint16_t address = 0x0000, lsb_address = 0x0000, msb_address = 0x0000;
    switch (selector) {
        case 0:
            loadPixel();
            tile_id = readPPUMemory(0x2000u | (vram_address.reg & 0x0FFFu));
            break;
        case 2:
            address = 0x23C0u | (vram_address.nametable_select_y << 11u) | (vram_address.nametable_select_x << 10u) | ((vram_address.coarse_y >> 2u) << 3u) | (vram_address.coarse_x >> 2u);
            tile_attribute = readPPUMemory(address);
            if(vram_address.coarse_y & 0x02u) tile_attribute >>= 4u;
            if(vram_address.coarse_x & 0x02u) tile_attribute >>= 2u;
            tile_attribute &= 0x03u;
            break;
        case 4:
            lsb_address = (ppuctrl.background_tile_select << 12u) + (uint16_t(tile_id) << 4u) + vram_address.fine_y;
            tile_lsb = readPPUMemory(lsb_address);
            break;
        case 6:
            msb_address = (ppuctrl.background_tile_select << 12u) + (uint16_t(tile_id) << 4u) + vram_address.fine_y + 8;
            tile_msb = readPPUMemory(msb_address);
            break;
        case 7:
            scrollingHorizontal();
            break;
        default:
            break;
    }
}

ppu2C02::Palette ppu2C02::getComposition() const {
    Palette palette{};
    uint16_t selector = 0x8000u >> fine_x;
    palette.pixel_id = (((shifter_pattern_high & selector) > 0) << 1u) | ((shifter_pattern_low & selector) > 0);
    palette.palette_id = (((shifter_attribute_high & selector) > 0) << 1u) | ((shifter_attribute_low & selector) > 0);
    return palette;
}

ppu2C02::SpritePalette ppu2C02::getSpriteComposition() {
    return ppu2C02::SpritePalette();
}

ppu2C02::FinalPalette ppu2C02::getFinalComposition(ppu2C02::Palette palette, ppu2C02::SpritePalette spritePalette) {
    return ppu2C02::FinalPalette();
}
