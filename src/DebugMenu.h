#pragma once

#include "Menu.h"

namespace Engine
{
    class Game;

    class DebugMenu: public Menu
    {
    public:
        DebugMenu(Game &_game);

        bool render(Menu *& /* unused */, bool & /* unused */) override;

    private:
        /**
         * Reference to Game to adjust settings.
         */
        Game &game;
    };
}