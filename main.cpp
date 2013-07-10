//
//  main.cpp
//  rnes
//
//  Created by Riley Andrews on 6/8/13.
//  Copyright (c) 2013 Riley Andrews. All rights reserved.
//

#include <stdlib.h>
#include "nes.h"

// TODO:
// - mmc1
// - sprite0 flag
// - clipping flags
// - apu
// - color emphasis
// - rename display class 

int main(int argc, char * argv[])
{
    Nes *nes = new Nes();
    int res = nes->loadRom(std::string("smb.nes"));
    if (res) {
        exit(1);
    }
    nes->run();
    delete nes;
    return 0;
}

