//
//  main.cpp
//  rnes
//
//  Created by Riley Andrews on 6/8/13.
//  Copyright (c) 2013 Riley Andrews. All rights reserved.
//

#include <stdlib.h>
#include <string>
#include <iostream>
#include "nes.h"

// TODO:
// - mmcs: nrom, mmc5
// - redo sprite/bg ordering. It's currently broken.
// - sprite0 flag
// - clipping flags
// - apu dmc channel
// - color emphasis

using namespace std;


std::string help = {
    "--rom [filename]\n"
    "-r [filename]\n"
};


void displayHelpAndQuit()
{
    cerr << help;
    exit(1);
}

int main(int argc, char *argv[])
{
    string romFile;
    bool romFileSpecified = false;

    for (int i = 0; i < argc; i++) {
        if (argv[i] == std::string("--rom") || argv[i] == std::string("-r")) {
            romFile = argv[i+1];
            romFileSpecified = true;
            i += 2;
        } 
    }

    if (!romFileSpecified) {
        displayHelpAndQuit();
    }

    Nes *nes = new Nes();
    int res = nes->loadRom(romFile);
    if (res) {
        cerr << "Failed to load rom: "<< romFile << "\n";
        exit(1);
    }
    nes->run();
    delete nes;
    return 0;
}

