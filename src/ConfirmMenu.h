#pragma once

#include "Menu.h"

#include <functional>

namespace Engine
{
    class ConfirmMenu: public Menu
    {
    public:
        ConfirmMenu(Menu *_parent_menu,
                    const std::function<void(bool)> &_on_choice_callback,
                    const char *_message,
                    const bool default_option);

        bool render(Menu *& /* unused */, bool &exit) override;

        void on_exit(Menu *&next_menu) override;

    private:
        /**
         * Parent menu to return to if no option is chosen.
         */
        Menu *parent_menu;

        /**
         * Callback to call when an option is chosen where the argument is true for "Yes" and false
         * for "No".
         */
        std::function<void(bool)> on_choice_callback;

        /**
         * Message to display.
         */
        const char *message;

        /**
         * Whether "Yes" was chosen. Only valid if @p is_option_chosen is true.
         */
        bool option;

        /**
         * Whether an option was chosen.
         */
        bool is_option_chosen;
    };
}