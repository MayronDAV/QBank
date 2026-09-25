#pragma once
#include "Panel.h"


namespace QB
{
    class StatsPanel : public Panel
    {
    public:
        StatsPanel();
        ~StatsPanel() override = default;

        void OnImGui() override;
    };


} // namespace QB