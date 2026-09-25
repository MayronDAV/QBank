#pragma once
#include "Panel.h"


namespace QB
{
    class ImagesPanel : public Panel
    {
        public:
            ImagesPanel();
            ~ImagesPanel() override = default;

            void OnImGui() override;
    };


} // namespace QB