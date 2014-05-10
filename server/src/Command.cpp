#include "Command.h"

#include "support/CommandLineParser.h"

#include <algorithm>
#include <cassert>
#include <functional>
#include <iostream>

namespace {

struct StringConverter {
  StringConverter(std::string *dest) : dest_(dest) {
  }

  bool operator()(const std::string &str) {
    *dest_ = str;
    return true;
  }

private:
  std::string *dest_;
};

struct UnsignedIntConverter {
  UnsignedIntConverter(unsigned *dest) : dest_(dest) {
  }

  bool operator()(const std::string &str) {
    try {
      int num = std::stoi(str);
      if (num > 0) {
        *dest_ = num;
        return true;
      }
    }
    catch (...) {
    }
    return false;
  }

private:
  unsigned *dest_;
};

} // unnamed namespace

std::ostream &operator<<(std::ostream &os, const Command::Action &action) {
  os << "Command::";

  switch (action) {
#define X(sym, str, help)                                                      \
  case Command::sym:                                                           \
    os << #sym;                                                                \
    break;
#include "Commands.def"
  }
  return os;
}

std::ostream &operator<<(std::ostream &os, const Command &command) {
  os << "Command{action=" << command.action << ", "
     << "file='" << command.file << "', "
     << "line='" << command.line << "', "
     << "column='" << command.column << "', "
     << "flags=[";
  bool first = true;
  for (const std::string &flag : command.flags) {
    if (!first)
      os << ", ";
    os << flag;
    first = false;
  }
  os << "]";

  return os << "}";
}

static Command::Action actionFromString(const std::string &actionStr) {
#define X(sym, str, help)                                                      \
  if (actionStr == str)                                                        \
    return Command::sym;

#include "Commands.def"

  return Command::Unknown;
}

CommandParser::CommandParser() : tempFile_("irony-server") {
}

Command *CommandParser::parse(const std::vector<std::string> &argv) {
  command_.clear();

  auto end = std::find(argv.begin(), argv.end(), "--");

  if (end != argv.end())
    command_.flags.assign(end + 1, argv.end());

  if (argv.begin() == end) {
    std::clog << "error: no command specified.\n"
                 "See 'irony-server help' to list available commands\n";
    return 0;
  }

  const std::string &actionStr = argv[0];

  command_.action = actionFromString(actionStr);

  bool handleUnsaved = false;
  std::vector<std::function<bool(const std::string &)>> positionalArgs;

  switch (command_.action) {
  case Command::CheckCompile:
    positionalArgs.push_back(StringConverter(&command_.file));
    handleUnsaved = true;
    break;

  case Command::Complete:
    positionalArgs.push_back(StringConverter(&command_.file));
    positionalArgs.push_back(UnsignedIntConverter(&command_.line));
    positionalArgs.push_back(UnsignedIntConverter(&command_.column));
    handleUnsaved = true;
    break;

  case Command::Help:
  case Command::Exit:
    break;

  case Command::Unknown:
    std::clog << "error: invalid command specified: " << actionStr << "\n";
    return 0;
  }

  auto argIt = argv.begin() + 1;
  int argCount = std::distance(argIt, end);

  // parse optional arguments come first
  while (argIt != end) {
    // '-' is allowed as a "default" file, this isn't an option but a positional
    // argument
    if ((*argIt)[0] != '-' || *argIt == "-")
      break;

    const std::string &opt = *argIt;

    ++argIt;
    argCount--;

    if (handleUnsaved) {
      // TODO: handle multiple unsaved files
      if (opt == "--num-unsaved=1") {
        command_.unsavedFiles.resize(1);
      }
    } else {
      std::clog << "error: invalid option for '" << actionStr << "': '" << opt
                << "' unknown\n";
      return 0;
    }
  }

  if (argCount != static_cast<int>(positionalArgs.size())) {
    std::clog << "error: invalid number of arguments for '" << actionStr
              << "' (requires " << positionalArgs.size() << " got " << argCount
              << ")\n";
    return 0;
  }

  for (auto fn : positionalArgs) {
    if (!fn(*argIt)) {
      std::clog << "error: invalid argument " << *argIt << " for '" << actionStr
                << "\n";
      return 0;
    }
    ++argIt;
  }

  // '-' is used as a special file to inform that the buffer hasn't been saved
  // on disk and only the buffer content is available. libclang needs a file, so
  // this is treated as a special value for irony-server to create a temporary
  // file for this. note taht libclang will gladly accept '-' as a filename but
  // we don't want to let this happen since irony already reads stdin.
  if (command_.file == "-") {
    command_.file = tempFile_.getPath();
  }

  // When a file is provided, the next line contains the compilation options to
  // pass to libclang.
  if (!command_.file.empty()) {
    std::string compileOptions;
    std::getline(std::cin, compileOptions);

    command_.flags = unescapeCommandLine(compileOptions);
  }

  // read unsaved files
  // filename
  // filesize
  // <file content...>
  for (auto &p : command_.unsavedFiles) {
    std::getline(std::cin, p.first);

    unsigned length;
    std::string filesizeStr;
    std::getline(std::cin, filesizeStr);

    UnsignedIntConverter uintConverter(&length);

    if (!uintConverter(filesizeStr)) {
      std::clog << "error: invalid file size '" << filesizeStr << "'\n";
      return 0;
    }

    p.second.resize(length);
    std::cin.read(p.second.data(), p.second.size());

    CXUnsavedFile cxUnsavedFile;

    cxUnsavedFile.Filename = p.first.c_str();
    cxUnsavedFile.Contents = p.second.data();
    cxUnsavedFile.Length = p.second.size();
    command_.cxUnsavedFiles.push_back(cxUnsavedFile);

    char nl;
    std::cin.read(&nl, 1);
    if (nl != '\n') {
      std::clog << "error: missing newline for unsaved file content\n";
      return 0;
    }
  }

  return &command_;
}