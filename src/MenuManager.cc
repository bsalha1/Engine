#include "MenuManager.h"

#include "assert_util.h"

namespace Engine
{
    /**
     * @brief Constructor.
     */
    MenuManager::MenuManager(): num_menus_stacked(0)
    {}

    /**
     * @brief Pushes a menu onto the stack.
     *
     * @param menu Menu to push.
     *
     * @return True if menu was pushed, otherwise false.
     */
    bool MenuManager::push_menu(Menu *menu)
    {
        ASSERT_RET_IF(num_menus_stacked >= max_num_menus_stacked, false);

        menu_stack[num_menus_stacked++] = menu;

        return true;
    }

    /**
     * @brief Renders the top menu on the stack. If it transitions to another menu, push it to
     * the stack.
     *
     * @return True if rendering succeeded, otherwise false.
     */
    bool MenuManager::render()
    {
        ASSERT_RET_IF_NOT(num_menus_stacked > 0, false);

        /*
         * Render the top menu.
         */
        bool exit = false;
        Menu *next_menu = nullptr;
        ASSERT_RET_IF_NOT(menu_stack[num_menus_stacked - 1]->render(next_menu, exit), false);

        /*
         * If exiting this menu, pop it off the stack.
         */
        if (exit)
        {
            ASSERT_RET_IF_NOT(pop_menu(), false);
        }

        /*
         * If transitioning to another menu, push it onto the stack.
         */
        if (next_menu != nullptr)
        {
            ASSERT_RET_IF_NOT(push_menu(next_menu), false);
        }

        return true;
    }

    /**
     * @brief Pops the top menu off the stack, calling its on_exit() method. It may spawn a new
     * menu.
     *
     * @return True on success, otherwise false.
     */
    bool MenuManager::pop_menu()
    {
        ASSERT_RET_IF(num_menus_stacked == 0, false);

        /*
         * Call the top menu's on_exit() method and pop it off.
         */
        Menu *next_menu = nullptr;
        menu_stack[num_menus_stacked - 1]->on_exit(next_menu);
        num_menus_stacked--;

        /*
         * If a new menu was spawned, push it onto the stack.
         */
        if (next_menu != nullptr)
        {
            push_menu(next_menu);
        }

        return true;
    }

    /**
     * @brief Pops the top menu off the stack, calling its on_exit() method. It may spawn a new
     * menu. Returns whether there are menus left on the stack after popping.
     *
     * @param[out] are_menus_left True if there are menus left on the stack after popping.
     *
     * @return True on success, otherwise false.
     */
    bool MenuManager::pop_menu(bool &are_menus_left)
    {
        ASSERT_RET_IF_NOT(pop_menu(), false);

        are_menus_left = num_menus_stacked != 0;

        return true;
    }
}