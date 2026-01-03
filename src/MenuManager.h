#pragma once

#include "Menu.h"

#include <array>
#include <cstddef>

namespace Engine
{
    class MenuManager
    {
    public:
        MenuManager();

        bool push_menu(Menu *menu);

        bool render();

        bool pop_menu();

        bool pop_menu(bool &are_menus_left);

    private:
        /**
         * Menu stack.
         * @(
         */

        /**
         * Maximum number of menus which can be stacked.
         */
        static constexpr size_t max_num_menus_stacked = 3;

        /**
         * Stack of menus.
         */
        std::array<Menu *, max_num_menus_stacked> menu_stack;

        /**
         * Number of menus currently stacked.
         */
        size_t num_menus_stacked;

        /**
         * @}
         */
    };
}