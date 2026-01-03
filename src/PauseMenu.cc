#include "PauseMenu.h"

#include "Game.h"

#include <imgui.h>

namespace Engine
{
    /**
     * @brief Constructor.
     */
    PauseMenu::PauseMenu(Game &_game, Renderer &renderer): game(_game), settings_menu(renderer)
    {}

    /**
     * @brief Render the menu, returning a pointer to the next menu to transition to, if any,
     * otherwise nullptr.
     *
     * @param[out] next_menu Pointer to the next menu to transition to, if any.
     *
     * @return True if rendering succeeded, otherwise false.
     */
    bool PauseMenu::render(Menu *&next_menu, bool & /* unused */)
    {
        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::Begin("Pause Menu",
                     nullptr,
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse |
                         ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoSavedSettings);
        ImGui::Text("Press ESC to unpause");

        /*
         * Add settings button.
         */
        ImGui::Separator();
        if (ImGui::Button("Settings"))
        {
            next_menu = &settings_menu;
            LOG_DEBUG("Pause Menu: Settings\n");
        }

        /*
         * Add quit button.
         */
        ImGui::Separator();
        if (ImGui::Button("Quit"))
        {
            game.quit();
            LOG_DEBUG("Pause Menu: Quit\n");
        }
        ImGui::End();

        return true;
    }

    /**
     * @brief Called when exiting the menu - does nothing for the Pause Menu.
     */
    void PauseMenu::on_exit(Menu *& /* unused */)
    {}
}