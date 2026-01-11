#pragma once

namespace Engine
{
    /**
     * @brief Abstract base class for menus.
     */
    class Menu
    {
    public:
        /**
         * @brief Render the menu, returning a pointer to the next menu to transition to, if any,
         * otherwise nullptr.
         *
         * @param[out] next_menu Pointer to the next menu to transition to, if any. Otherwise
         * nullptr.
         * @param[out] exit Whether to exit this menu.
         *
         * @note @p next_menu and @p exit are not mutually exclusive, setting both will result
         * in replacing the current menu with @p next_menu.
         *
         * @return True if rendering succeeded, otherwise false.
         */
        virtual bool render(Menu *&next_menu, bool &exit) = 0;

        /**
         * @brief Called when exiting the menu. By default, does nothing.
         *
         * @param[out] next_menu Pointer to the next menu to transition to, if any. Otherwise
         * nullptr.
         */
        virtual void on_exit(Menu *&next_menu)
        {}
    };
}