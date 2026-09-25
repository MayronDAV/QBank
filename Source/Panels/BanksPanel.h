#pragma once
#include "Panel.h"


namespace QB
{
    class BanksPanel : public Panel
    {
        public:
            BanksPanel();
            ~BanksPanel() override = default;

            void OnImGui() override;
    };


} // namespace QB