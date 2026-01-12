#pragma once

#include "Menu.h"
#include "Renderer.h"

namespace Engine
{
    class Game;

    class DebugMenu: public Menu
    {
    public:
        DebugMenu(Game &_game, Renderer &_renderer);

        bool render(Menu *& /* unused */, bool & /* unused */) override;

    private:
        /**
         * Reference to Game to adjust settings.
         */
        Game &game;

        /**
         * Reference to Renderer to adjust settings.
         */
        Renderer &renderer;
    };
}