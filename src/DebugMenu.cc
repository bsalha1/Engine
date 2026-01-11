#include "DebugMenu.h"

#include "Game.h"

#include <imgui.h>

namespace Engine
{
    /**
     * @brief Constructor.
     */
    DebugMenu::DebugMenu(Game &_game): game(_game)
    {}

    /**
     * @brief Render the menu.
     *
     * @return True if rendering succeeded, otherwise false.
     */
    bool DebugMenu::render(Menu *& /* unused */, bool & /* unused */)
    {
        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::Begin("Debug Menu",
                     nullptr,
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse |
                         ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoSavedSettings);
        ImGui::Text("Press ESC to go back");

        float game_time_per_real_time = static_cast<float>(game.game_time_per_real_time);
        ImGui::SliderFloat("Game Time per Real Time", &game_time_per_real_time, 0.1f, 10.f);
        game.game_time_per_real_time = static_cast<double>(game_time_per_real_time);

        ImGui::End();

        return true;
    }
}