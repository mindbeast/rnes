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
#include <memory>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <crypt.h>
#include <sys/types.h>
#include <pwd.h>
#include "save.pb.h"
#include "nes.h"


// TODO:
// - mmcs: nrom, mmc5
// - redo sprite/bg ordering. It's currently broken.
// - sprite0 flag
// - clipping flags
// - apu dmc channel
// - color emphasis

std::string help = {
    "--rom [filename]\n"
    "-r [filename]\n"
};

void displayHelpAndQuit()
{
    std::cerr << help;
    exit(1);
}

std::string md5OfFile(std::string file) 
{
    std::string salt = "$1$$";
    struct crypt_data data = {0};
    std::string contents;

    using namespace std;
    namespace io = boost::iostreams;

    io::stream<io::mapped_file_source> stream(file);
    stream >> contents;
    
    auto ret = std::string(crypt_r(contents.c_str(), salt.c_str(), &data));
    return ret.substr(salt.size());
}

std::string getUserHomeDir()
{
    struct passwd pwd = {0};
    struct passwd *pwdRet;
    char buf[1024];

    getpwuid_r(getuid(), &pwd, buf, sizeof(buf), &pwdRet);  
    
    using namespace std;
    string homeDir(pwdRet->pw_dir);
    return homeDir;
}

void setupDirectories(std::string romMd5) 
{
    namespace fs = boost::filesystem;

    fs::path homeDir(getUserHomeDir());
    fs::path rnesDir(homeDir);
    rnesDir /= ".rnes";

    if (not fs::exists(rnesDir)) {
        fs::create_directories(rnesDir);
    }

    fs::path gamePath(rnesDir);
    gamePath /= fs::path(romMd5);
    if (not fs::exists(gamePath)) {
        fs::create_directories(gamePath);
    }
}

void verifyRomExists(std::string romFile)
{
    namespace fs = boost::filesystem;
    using namespace std;

    fs::path path(romFile);
    if (not fs::exists(path) or not fs::is_regular_file(path)) {
        cerr << "specified rom file does not exist: " << romFile << endl;
        exit(-1);
    }
    cerr << "rom md5: " << md5OfFile(romFile) << endl;
}

int main(int argc, char *argv[])
{
    using namespace std;
    namespace fs = boost::filesystem;
    string romFile;
    bool romFileSpecified = false;

    // Verify that the version of the library that we linked against is
    // compatible with the version of the headers we compiled against.
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    // TODO: convert argument parsing to boost program options
    for (int i = 0; i < argc; i++) {
        if (argv[i] == string("--rom") || argv[i] == string("-r")) {
            romFile = argv[i+1];
            romFileSpecified = true;
            i += 2;
        } 
    }

    if (!romFileSpecified) {
        displayHelpAndQuit();
    }

    try {
        // verify the rom file exists.
        verifyRomExists(romFile);

        using namespace Rnes;
        auto nes = unique_ptr<Nes>{new Nes{}};
        int res = nes->loadRom(romFile);
        if (res) {
            cerr << "Failed to load rom: "<< romFile << endl;
            exit(1);
        }

        // Setup the rnes directories.
        setupDirectories(md5OfFile(romFile));
    
        // Start the emulator loop.
        nes->run();
    }
    catch (const fs::filesystem_error& exception) {
        cerr << "Exception: " << exception.what() << endl;
        return -1;
    }
    catch (...) {
        cerr << "Unknown Exception!" << endl;
        return -1;
    }
    return 0;
}


