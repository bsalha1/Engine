#include "SettingsMenu.h"

#include <imgui.h>

namespace Engine
{
    /**
     * @brief Constructor.
     */
    SettingsMenu::SettingsMenu(Renderer &renderer):
        renderer(renderer),
        apply_confirm(
            this, /* parent_menu */
            [this](const bool apply) { on_apply_confirm(apply); } /* on_choice_callback */,
            "Apply Settings?" /* message */,
            false /* default_option */),
        applied_settings {},
        working_settings {}
    {}

    /**
     * @brief Render the menu.
     *
     * @return True if rendering succeeded, otherwise false.
     */
    bool SettingsMenu::render(Menu *& /* unused */, bool & /* unused */)
    {
        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::Begin("Settings Menu",
                     nullptr,
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse |
                         ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoSavedSettings);
        ImGui::Text("Press ESC to go back");

        float exposure = renderer.get_exposure();
        ImGui::SliderFloat("Exposure", &exposure, 0.f, 10.f);
        ASSERT_RET_IF_NOT(renderer.set_exposure(exposure), false);

        float gamma = renderer.get_gamma();
        ImGui::SliderFloat("Gamma", &gamma, 0.f, 10.f);
        ASSERT_RET_IF_NOT(renderer.set_gamma(gamma), false);

        float sharpness = renderer.get_sharpness();
        ImGui::SliderFloat("Sharpness", &sharpness, 1.f, 1000.f);
        ASSERT_RET_IF_NOT(renderer.set_sharpness(sharpness), false);

        ImGui::Checkbox("V-Sync", &working_settings.vsync_enabled);

        if (ImGui::Button("Apply Settings"))
        {
            apply_settings();
        }

        ImGui::End();

        return true;
    }

    /**
     * @brief Apply the working settings.
     */
    void SettingsMenu::apply_settings()
    {
        glfwSwapInterval(working_settings.vsync_enabled ? 1 : 0);

        applied_settings = working_settings;
    }

    /**
     * @brief Called when there was a choice in the apply confirmation menu.
     *
     * @param apply Whether to apply the settings.
     */
    void SettingsMenu::on_apply_confirm(const bool apply)
    {
        /*
         * If applying, apply the settings.
         */
        if (apply)
        {
            apply_settings();
        }

        /*
         * Otherwise, revert working settings to applied settings.
         */
        else
        {
            working_settings = applied_settings;
        }
    }

    /**
     * @brief Called when exiting the menu.
     *
     * @param[out] next_menu Pointer to apply confirmation menu if there are unapplied settings.
     */
    void SettingsMenu::on_exit(Menu *&next_menu)
    {
        /*
         * If there are unapplied settings, prompt the user to apply them.
         */
        if (memcmp(&applied_settings, &working_settings, sizeof(AppliableSettings)) != 0)
        {
            next_menu = &apply_confirm;
        }
    }
}