#pragma once

#include "DebugMenu.h"
#include "Menu.h"
#include "Renderer.h"
#include "SettingsMenu.h"

namespace Engine
{
    class Game;

    class PauseMenu: public Menu
    {
    public:
        PauseMenu(Game &_game, Renderer &renderer);

        bool render(Menu *&next_menu, bool & /* unused */) override;

    private:
        /**
         * Reference to the Game instance to apply game-wide changes to.
         */
        Game &game;

        /**
         * Settings menu.
         */
        SettingsMenu settings_menu;

        /**
         * Debug menu.
         */
        DebugMenu debug_menu;
    };
}