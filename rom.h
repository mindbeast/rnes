//
//  rom.h
//  rnes
//
//  Created by Riley Andrews on 3/17/14.
//  Copyright (c) 2014 Riley Andrews. All rights reserved.
//

#ifndef __ROM_H__
#define __ROM_H__

#include <vector>
#include <cstdint>

namespace Rnes {

enum class MmcType {
    MMC_TYPE_NONE, 
    MMC_TYPE_1,
    MMC_TYPE_2,
    MMC_TYPE_3,
};

class Rom {
    std::vector<const uint8_t*> prgRom;
    std::vector<const uint8_t*> chrRom;
    MmcType mmcType;

public:
    const uint8_t *getChrRom(int romNum);
    const uint8_t *getPrgRom(int romNum);
    int getChrRomCount();
    int getPrgRomCount();
    MmcType getMmcType();
    
    Rom() {}
    ~Rom() {}
    Rom(const Rom &) = delete;
};

};

#endif 
