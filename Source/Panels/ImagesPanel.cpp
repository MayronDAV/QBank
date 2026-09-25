#include "ImagesPanel.h"
#include "Application.h"
#include "Utils.h"

// lib
#include <imgui.h>
#include <imgui_internal.h>



namespace QB
{
    ImagesPanel::ImagesPanel()
        : Panel("##ImagesWindow", false)
    {
    }

    void ImagesPanel::OnImGui()
    {
        if (!m_Active) return;

        auto bank                     = Application::Get().GetCurrentBank();
        if (!bank) return;

        ImGui::SetNextWindowSizeConstraints({ 350.0f, 450.0f }, { FLT_MAX, FLT_MAX });
        ImGui::Begin(m_Name.c_str(), &m_Active, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse);

        constexpr ImVec2 viewportSize = { 100.0f, 100.0f };
        const float panelWidth        = ImGui::GetContentRegionAvail().x;
        const ImVec2 cellSize         = viewportSize + ImVec2(PADDING, PADDING);

        int columnCount               = static_cast<int>(panelWidth / cellSize.x);
        columnCount                   = std::max(1, columnCount);

        std::vector<uint64_t> images;
        images.reserve(bank->Images.size());

        for (const auto& [handle, image] : bank->Images)
        {
            if (image)
                images.push_back(handle);
        }

        std::vector<uint64_t> imagesToRemove;

        if (ImGui::BeginTable("##ImagesTable", columnCount, ImGuiTableFlags_SizingFixedFit))
        {
            ImGuiListClipper clipper;
            const size_t imagesCount = images.size();
            const int rowCount       = static_cast<int>((imagesCount + columnCount - 1) / columnCount);
            const float rowHeight    = viewportSize.y + ImGui::GetTextLineHeight() * 2.0f + PADDING;

            clipper.Begin(rowCount, rowHeight);
            while (clipper.Step())
            {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
                {
                    ImGui::TableNextRow();

                    for (int column = 0; column < columnCount; column++)
                    {
                        const int index = row * columnCount + column;
                        if (index >= static_cast<int>(imagesCount))
                            continue;

                        ImGui::TableSetColumnIndex(column);

                        const uint64_t handle = images[index];
                        auto imageIt          = bank->Images.find(handle);
                        if (imageIt == bank->Images.end())
                            continue;

                        const std::shared_ptr<Image>& image = imageIt->second;
                        if (!image)
                            continue;

                        ImGui::PushID(static_cast<int>(index));

                        bool removeClicked = false;
                        ImageSelectButton(image, viewportSize, removeClicked);
                        if (removeClicked)
                        {
                            imagesToRemove.push_back(handle);
                        }

                        if (!removeClicked && ImGui::BeginDragDropSource())
                        {
                            ImGui::SetDragDropPayload("IMAGES_ITEM", &handle, sizeof(handle));
                            ImGui::EndDragDropSource();
                        }

                        ImGui::PopID();
                    }
                }
            }

            ImGui::EndTable();
        }

        for (const auto& handle : imagesToRemove)
        {
            if (handle)
            {
                bank->Images[handle] = nullptr;
                bank->Images.erase(handle);
            }
        }

        ImGui::End();
    }

} // namespace QB
