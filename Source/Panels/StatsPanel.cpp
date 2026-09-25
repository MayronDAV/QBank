#include "StatsPanel.h"
#include "Application.h"
#include "Utils.h"

// lib
#include <imgui.h>
#include <imgui_internal.h>



namespace QB
{
    StatsPanel::StatsPanel()
        : Panel("StatsWindow", false)
    {
    }

    void StatsPanel::OnImGui()
    {
        if (!m_Active) return;

        auto* bank = Application::Get().GetCurrentBank();
        if (!bank) return;

        auto& status = bank->CurStatus;

        ImGui::SetNextWindowSizeConstraints({ 350, 450 }, { FLT_MAX, FLT_MAX });
        ImGui::Begin(m_Name.c_str(), &m_Active, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);

        const auto size = ImGui::GetContentRegionAvail();
        ImGui::BeginChild("##StatusInfo", { 0.0f, size.y - 23.0f }, false);

        ImGui::Text("Respostas Corretas: %i/%i", status.CorrectAnswers, status.TotalAnswers);

        if (status.CorrectAnswers == status.TotalAnswers)
        {
            ImGui::TextColored({ 0.0f, 1.0f, 0.0f, 1.0f }, "Parabéns, você acertou todas as questões!!!");
        }
        else
        {
            if (ImGui::TreeNodeEx("Respostas Incorretas", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_FramePadding))
            {
                ImGui::BeginChild("##IncorrectAnswers", { 0.0f, 0.0f }, true);

                for (size_t i = 0; i < status.IncorrectQuestions.size(); ++i)
                {
                    auto& question = bank->Questions[i];
                    ImGui::PushID((int)i);

                    if (ImGui::TreeNodeEx(("Questão " + std::to_string(i + 1)).c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_Bullet))
                    {
                        float questionDetailsHeight = CalculateQuestionDetailsHeight(question, ImGui::GetContentRegionAvail().x);
                        questionDetailsHeight = question.Image != 0 ? questionDetailsHeight + QUESTION_IMAGE_HEIGHT : questionDetailsHeight;

                        ImGui::BeginChild("##QuestionDetails", ImVec2(0, questionDetailsHeight), true);
                        {
                            DrawCategoryButtons(question.Categories, ImGui::GetContentRegionAvail().x, false);

                            if (question.Image != 0 && bank->Images[question.Image])
                            {
                                float width = ImGui::GetContentRegionAvail().x;

                                ImGui::BeginGroup();
                                ImGui::PushID((int)question.Image);

                                ImVec2 viewport = { width, QUESTION_IMAGE_HEIGHT };
                                auto& image = bank->Images[question.Image];

                                ImVec2 cursor = ImGui::GetCursorScreenPos();
                                ImDrawList* drawList = ImGui::GetWindowDrawList();

                                ImU32 backgroundColor = IM_COL32(30, 30, 30, 255);

                                drawList->AddRectFilled(
                                    cursor,
                                    cursor + viewport,
                                    backgroundColor,
                                    4.0f);

                                float imageWidth = static_cast<float>(image->GetWidth());
                                float imageHeight = static_cast<float>(image->GetHeight());

                                float targetAspect = imageWidth / imageHeight;
                                float viewportAspect = viewport.x / viewport.y;

                                ImVec2 imageSize;
                                if (viewportAspect > targetAspect)
                                {
                                    imageSize.y = viewport.y;
                                    imageSize.x = imageSize.y * targetAspect;
                                }
                                else
                                {
                                    imageSize.x = viewport.x;
                                    imageSize.y = imageSize.x / targetAspect;
                                }

                                ImVec2 imagePos;
                                imagePos.x = cursor.x + (viewport.x - imageSize.x) * 0.5f;
                                imagePos.y = cursor.y + (viewport.y - imageSize.y) * 0.5f;

                                drawList->AddImage(
                                    (ImTextureID)image->GetID(),
                                    imagePos,
                                    imagePos + imageSize
                                );

                                ImGui::Dummy(imageSize);

                                ImGui::PopID();
                                ImGui::EndGroup();
                            }

                            if (!question.Text.empty())
                            {
                                float questionInfoHeight = CalculateWrappedTextHeight(question.Text, ImGui::GetContentRegionAvail().x, PADDING);
                                ImGui::BeginChild("##QuestionInfo", ImVec2(0, questionInfoHeight), true);

                                ImGui::SetCursorPos(ImVec2(PADDING, PADDING));
                                ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x - PADDING);
                                ImGui::TextUnformatted(question.Text.c_str());
                                ImGui::PopTextWrapPos();

                                ImGui::EndChild();
                            }

                            if (ImGui::BeginTable("##AnswerOptions", 3, ImGuiTableFlags_SizingStretchProp))
                            {
                                ImGui::TableSetupColumn("A", ImGuiTableColumnFlags_WidthStretch);
                                ImGui::TableSetupColumn("B", ImGuiTableColumnFlags_WidthStretch);
                                ImGui::TableSetupColumn("C", ImGuiTableColumnFlags_WidthStretch);

                                const int optionCount = question.CurrentOptionIndex;
                                for (int i = 0; i < optionCount; i += 3)
                                {
                                    ImGui::TableNextRow();

                                    for (size_t column = 0; column < 3; column++)
                                    {
                                        const size_t optionIndex = i + column;

                                        ImGui::TableNextColumn();

                                        if (optionIndex >= optionCount)
                                            continue;

                                        auto index = static_cast<int>(optionIndex);
                                        ImGui::PushID(index);

                                        if (optionIndex == question.CorrectOptionIndex)
                                            ImGui::PushStyleColor(ImGuiCol_Text, { 0.0f, 1.0f, 0.0f, 1.0f });
                                        else if (optionIndex == question.OptionMarkedIndex)
                                            ImGui::PushStyleColor(ImGuiCol_Text, { 1.0f, 0.0f, 0.0f, 1.0f });

                                        ImGui::Text("(%c)", 'A' + static_cast<char>(optionIndex));
                                        ImGui::SameLine();
                                        ImGui::PushTextWrapPos();
                                        ImGui::TextUnformatted(question.Options[optionIndex].c_str());
                                        ImGui::PopTextWrapPos();

                                        if (optionIndex == question.CorrectOptionIndex || optionIndex == question.OptionMarkedIndex)
                                            ImGui::PopStyleColor();

                                        ImGui::PopID();
                                    }
                                }
                                ImGui::EndTable();
                            }
                        }
                        ImGui::EndChild();

                        if (question.OptionMarkedIndex != question.CorrectOptionIndex && !question.Explanation.empty())
                        {
                            ImGui::Text("Explicação:");

                            float height = CalculateWrappedTextHeight(question.Explanation, ImGui::GetContentRegionAvail().x, PADDING);
                            ImGui::BeginChild("##Explanation", ImVec2(0, height), true);

                            ImGui::SetCursorPos(ImVec2(PADDING, PADDING));

                            ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x - PADDING);
                            ImGui::TextUnformatted(question.Explanation.c_str());
                            ImGui::PopTextWrapPos();

                            ImGui::EndChild();
                        }

                        ImGui::TreePop();
                    }

                    ImGui::PopID();
                }

                ImGui::EndChild();
                ImGui::TreePop();
            }
        }

        ImGui::EndChild();

        // TODO: Add a way to save the status of each attempt and show it somewhere
        if (ImGui::Button("Fechar", ImVec2(100, 0)))
        {
            bank->CurStatus = {};
            for (auto& question : bank->Questions)
            {
                question.OptionMarkedIndex = -1;
            }
            m_Active = false;
        }

        ImGui::End();
    }
}
