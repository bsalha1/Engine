#include "ConfirmMenu.h"

#include <imgui.h>

namespace Engine
{
    /**
     * @brief Constructor.
     */
    ConfirmMenu::ConfirmMenu(Menu *_parent_menu,
                             const std::function<void(bool)> &_on_choice_callback,
                             const char *_message,
                             const bool default_option):
        parent_menu(_parent_menu),
        on_choice_callback(_on_choice_callback),
        message(_message),
        option(default_option),
        is_option_chosen(false)
    {}

    /**
     * @brief Render the menu.
     *
     * @param[out] exit Whether to exit this menu.
     *
     * @return True if rendering succeeded, otherwise false.
     */
    bool ConfirmMenu::render(Menu *& /* unused */, bool &exit)
    {
        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::Begin("Confirm Menu",
                     nullptr,
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse |
                         ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoSavedSettings);

        ImGui::Text(message);

        is_option_chosen = false;

        if (ImGui::Button("Yes"))
        {
            option = true;
            is_option_chosen = true;
        }
        else if (ImGui::SameLine(); ImGui::Button("No"))
        {
            option = false;
            is_option_chosen = true;
        }

        /*
         * Exit if an option was chosen.
         */
        exit = is_option_chosen;

        ImGui::End();

        return true;
    }

    /**
     * @brief Called when exiting the menu.
     *
     * @param[out] next_menu Pointer to the next menu to transition to, if any.
     */
    void ConfirmMenu::on_exit(Menu *&next_menu)
    {
        /*
         * If an option was chosen, invoke the callback with the chosen option.
         */
        if (is_option_chosen)
        {
            on_choice_callback(option);
        }

        /*
         * Otherwise, If no option was chosen, go back to parent menu.
         */
        else
        {
            next_menu = parent_menu;
        }
    }
}