#include "hook.h"

#include <string>
#include <vector>

// cxhook.exe. Claude Code runs this with no argument on every hook event in
// every session; a person or the installer runs it with one of three verbs.
//
// Nothing happens before the dispatch. There is no config to load, no PATH to
// probe and no tree to scan, which is the point: this process is started
// several times a second on a busy machine and the only thing that makes that
// affordable is that it does nothing on the way in.
int main(int argc, char** argv)
{
    std::vector<std::string> args;
    args.reserve(argc > 1 ? static_cast<std::size_t>(argc - 1) : 0);
    for (int i = 1; i < argc; ++i)
        args.emplace_back(argv[i]);

    return cx::hook::run(args);
}
