#include "BanksPanel.h"
#include "Application.h"
#include "Utils.h"

// lib
#include <imgui.h>
#include <imgui_internal.h>



namespace QB
{
    BanksPanel::BanksPanel()
        : Panel("Bancos", false)
    {
    }

    void BanksPanel::OnImGui()
    {
        if (!m_Active) return;

        auto& session = Application::Get().GetSession();

        int windowX, windowY;
        glfwGetWindowPos(Application::Get().GetWindow(), &windowX, &windowY);

        const float frameHeight = ImGui::GetFrameHeight();
        const float x           = static_cast<float>(windowX) + SIDEBAR_WIDTH;
        const float y           = static_cast<float>(windowY) + frameHeight;

        ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);

        const float width       = static_cast<float>(session.Width) / 4.0f;
        const float height      = static_cast<float>(session.Height) - frameHeight;

        ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);

        auto color = ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg);
        ImGui::PushStyleColor(ImGuiCol_TitleBg, color);
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, color);
        ImGui::Begin(m_Name.c_str(), &m_Active, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
        ImGui::PopStyleColor(2);

        auto size = ImGui::GetContentRegionAvail().x;
        std::string bankToDelete = "";
        for (const auto& path : session.Banks)
        {

            ImGui::PushID(path.c_str());

            auto absPath = std::filesystem::absolute(path).lexically_normal();
            auto name = absPath.stem().string();
            ImGui::BeginDisabled(session.StartBank == absPath.generic_string());
            if (ImGui::Button(name.c_str(), { size, 0.0f }))
            {
                //Application::Get().ImportBank(path);
                Application::Get().ChangeBank(path);
            }
            ImGui::EndDisabled();

            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem("Delete"))
                    bankToDelete = path;

                ImGui::EndPopup();
            }

            ImGui::PopID();
        }

        if (!bankToDelete.empty())
        {
            std::erase_if(session.Banks, [&](const auto& p_Path)
            {
                return p_Path == bankToDelete;
            });
        }

        ImGui::End();
    }

} // namespace QB
