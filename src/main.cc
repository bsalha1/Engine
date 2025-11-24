#include "Game.h"
#include "assert_util.h"
#include "log.h"

int main()
{
    LOG("Starting game\n");

    using namespace Engine;

    std::unique_ptr<Game> game = Game::create();
    ASSERT_RET_IF_NOT(game, false);

    game->run();

    game->end();

    return 0;
}