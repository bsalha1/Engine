#include "Game.h"
#include "assert_util.h"
#include "log.h"

#include <filesystem>
#include <fstream>

#ifndef GIT_COMMIT
#define GIT_COMMIT "unknown"
#endif

int main()
{
    LOG("Starting game (commit %s, cwd %s)\n", GIT_COMMIT, std::filesystem::current_path().string().c_str());
    using namespace Engine;

    std::unique_ptr<Game> game = Game::create();
    ASSERT_RET_IF_NOT(game, -1);

    game->run();

    return 0;
}