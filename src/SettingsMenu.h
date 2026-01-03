#pragma once

#include "ConfirmMenu.h"
#include "Menu.h"
#include "Renderer.h"

namespace Engine
{
    class SettingsMenu: public Menu
    {
    public:
        /**
         * Settings that shouldn't be applied until confirmed.
         */
        struct AppliableSettings
        {
            /**
             * Whether Vertical Sync (V-Sync) is enabled.
             */
            bool vsync_enabled;
        };

        SettingsMenu(Renderer &renderer);

        bool render(Menu *& /* unused */, bool & /* unused */) override;

        void apply_settings();

        void on_apply_confirm(const bool apply);

        void on_exit(Menu *&next_menu) override;

    private:
        /**
         * Reference to Renderer to adjust settings.
         */
        Renderer &renderer;

        /**
         * Confirmation menu for applying unapplied settings.
         */
        ConfirmMenu apply_confirm;

        /**
         * Applied settings.
         */
        AppliableSettings applied_settings;

        /**
         * Working settings that may not be applied yet.
         */
        AppliableSettings working_settings;
    };
}