//
//  main.cpp
//  rnes
//
//

#include <boost/filesystem.hpp>
#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/program_options.hpp>
#include <crypt.h>
#include <iostream>
#include <memory>
#include <pwd.h>
#include <stdlib.h>
#include <string>
#include <sys/types.h>

#include "nes.h"
#include "save.pb.h"

using namespace Rnes;

// TODO:
// - mmcs: nrom, mmc5
// - redo sprite/bg ordering. It's currently broken.
// - sprite0 flag
// - clipping flags
// - apu dmc channel
// - color emphasis

std::string help = {"--rom [filename]\n"
                    "-r [filename]\n"};

void displayHelpAndQuit() {
  std::cerr << help;
  exit(1);
}

namespace Rnes {
std::string md5OfFile(std::string file) {
  std::string salt = "$1$$";
  struct crypt_data data = {0};
  std::string contents;

  using namespace std;
  namespace io = boost::iostreams;

  io::stream<io::mapped_file_source> stream(file);
  stream >> contents;

  auto ret = std::string(crypt_r(contents.c_str(), salt.c_str(), &data));
  for_each(begin(ret), end(ret), [](char &c) {
    if (c == '/' or c == '.') {
      c = 'x';
    }
  });
  return ret.substr(salt.size());
}

std::string getUserHomeDir() {
  struct passwd pwd = {0};
  struct passwd *pwdRet;
  char buf[1024];

  getpwuid_r(getuid(), &pwd, buf, sizeof(buf), &pwdRet);

  using namespace std;
  string homeDir(pwdRet->pw_dir);
  return homeDir;
}

std::string getGameSaveDir(std::string &romFile) {
  std::string homeDir = getUserHomeDir();
  std::string md5 = md5OfFile(romFile);

  return homeDir + "/.rnes/" + md5 + "/save";
}

void setupDirectories(std::string romMd5) {
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

void verifyRomExists(std::string romFile) {
  namespace fs = boost::filesystem;
  using namespace std;

  fs::path path(romFile);
  if (not fs::exists(path) or not fs::is_regular_file(path)) {
    cerr << "specified rom file does not exist: " << romFile << endl;
    exit(-1);
  }
  cerr << "rom md5: " << md5OfFile(romFile) << endl;
}
void loadNesState(Nes *nes, std::string saveFile) {
  namespace io = boost::iostreams;
  namespace fs = boost::filesystem;
  using namespace std;

  try {
    io::stream<io::mapped_file_source> stream(saveFile);
    unique_ptr<SaveState> gameState{new SaveState{}};

    // Read out state from save file.
    gameState->ParseFromIstream(&stream);

    // Restore nes state.
    nes->restore(*gameState.get());

    cerr << "State restored" << endl;
  } catch (const std::ios_base::failure &exception) {
    cerr << "Exception: " << exception.what() << endl;
  }
}

void saveNesState(Nes *nes, std::string saveFile) {
  namespace io = boost::iostreams;
  namespace fs = boost::filesystem;
  using namespace std;

  // Useless comment.

  try {
    io::stream<io::file_sink> stream(saveFile);
    unique_ptr<SaveState> gameState{new SaveState{}};

    // Save nes state.
    nes->save(*gameState.get());

    // Serialize save to file.
    gameState->SerializeToOstream(&stream);

    cerr << "state saved." << endl;
  } catch (const std::ios_base::failure &exception) {
    cerr << "Exception: " << exception.what() << endl;
  }
}
} // namespace Rnes

int main(int argc, char *argv[]) {
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
      romFile = argv[i + 1];
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

    auto nes = unique_ptr<Nes>{new Nes{}};
    int res = nes->loadRom(romFile);
    if (res) {
      cerr << "Failed to load rom: " << romFile << endl;
      exit(1);
    }

    // Setup the rnes directories.
    setupDirectories(md5OfFile(romFile));

    // Start the emulator loop.
    nes->run();
  } catch (const fs::filesystem_error &exception) {
    cerr << "Exception: " << exception.what() << endl;
    return -1;
  } catch (const std::exception &exception) {
    cerr << "Exception: " << exception.what() << endl;
    return -1;
  } catch (...) {
    cerr << "Unknown Exception!" << endl;
    return -1;
  }

  google::protobuf::ShutdownProtobufLibrary();
  return 0;
}
