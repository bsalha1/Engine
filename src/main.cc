#include "Game.h"
#include "assert_util.h"
#include "log.h"

#ifndef GIT_COMMIT
#define GIT_COMMIT "unknown"
#endif

int main()
{
    LOG("Starting game (commit %s)\n", GIT_COMMIT);

    using namespace Engine;

    std::unique_ptr<Game> game = Game::create();
    ASSERT_RET_IF_NOT(game, -1);

    game->run();

    return 0;
}