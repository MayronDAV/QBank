#include "QuestionPanel.h"
#include "Application.h"
#include "Utils.h"
#include "FileDialog.h"

// lib
#include <imgui.h>
#include <imgui_internal.h>




namespace QB
{
    QuestionPanel::QuestionPanel()
        : Panel("##QuestionEditor", false)
    {
    }

    void QuestionPanel::OnImGui()
    {
        static bool wasQuestionWindowOpen = false;

        static Question question          = {};
        static char questionText[1024]    = {};
        static char explanationText[1024] = {};
        static char answerText[256]       = {};

        auto* bank                        = Application::Get().GetCurrentBank();

        const bool justOpened             = m_Active && !wasQuestionWindowOpen;
        if (!m_Active || !bank)
        {
            wasQuestionWindowOpen         = false;
            return;
        }

        auto& session                     = Application::Get().GetSession();

        if (justOpened)
        {
            question = {};

            std::memset(questionText, 0, sizeof(questionText));
            std::memset(explanationText, 0, sizeof(explanationText));
            std::memset(answerText, 0, sizeof(answerText));

            if (m_QuestionToEdit >= 0 && m_QuestionToEdit < static_cast<int>(bank->Questions.size()))
            {
                question = bank->Questions[m_QuestionToEdit];

                std::snprintf(questionText, sizeof(questionText), "%s", question.Text.c_str());
                std::snprintf(explanationText, sizeof(explanationText), "%s", question.Explanation.c_str());

                // Make sure CurrentOptionIndex is valid.
                question.CurrentOptionIndex = std::clamp(question.CurrentOptionIndex, 0, static_cast<int>(question.Options.size()));
                if (question.CurrentOptionIndex == 0)
                {
                    while (question.CurrentOptionIndex < static_cast<int>(question.Options.size()) && !question.Options[question.CurrentOptionIndex].empty())
                    {
                        question.CurrentOptionIndex++;
                    }
                }

                if (question.CorrectOptionIndex < -1 || question.CorrectOptionIndex >= question.CurrentOptionIndex)
                    question.CorrectOptionIndex = -1;

                if (question.CorrectOptionIndex >= 0)
                    question.CorrectOptionText = question.Options[question.CorrectOptionIndex];
                else
                    question.CorrectOptionText.clear();
            }
            else
            {
                question.CurrentOptionIndex = 0;
                question.CorrectOptionIndex = -1;
            }
        }

        ImGui::SetNextWindowSizeConstraints({ 400, 500 }, { FLT_MAX, FLT_MAX });
        ImGui::Begin(m_Name.c_str(), &m_Active, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);

        ImGui::BeginChild("##Child", ImVec2(0, ImGui::GetContentRegionAvail().y - 23.0f), true);

        const float width       = ImGui::GetContentRegionAvail().x;
        const ImVec2 buttonSize = { width, 100.0f };

        ImGui::BeginGroup();
        {
            std::shared_ptr<Image> image = Application::Get().GetWhiteImage();
            if (question.Image > 0)
            {
                auto it = bank->Images.find(question.Image);
                if (it != bank->Images.end() && it->second)
                    image = it->second;
            }

            ImGui::PushID("QuestionImage");

            const ImVec2 cursor = ImGui::GetCursorScreenPos();
            const bool clicked  = ImGui::InvisibleButton("##ImageButton", buttonSize);
            const bool hovered  = ImGui::IsItemHovered();
            const bool active   = ImGui::IsItemActive();

            static bool imageSelectorJustOpened = false;
            if (clicked)
            {
                ImGui::OpenPopup("ImageSelector");
                imageSelectorJustOpened = true;
            }

            if (ImGui::BeginPopup("ImageSelector"))
            {
                static std::shared_ptr<Image> clipImage = nullptr;
                static std::vector<std::shared_ptr<Image>> recentImages;
                const float previewSize                 = 60.0f;
                const float columnWidth                 = previewSize;
                const float popupWidth                  = 6.0f * columnWidth - PADDING * 2.0f;

                if (imageSelectorJustOpened || !clipImage)
                {
                    clipImage                           = nullptr;
                    auto newImage                       = Application::Get().GetClipboardImage();
                    if (newImage)
                        clipImage                       = newImage;

                    recentImages.clear();
                    recentImages.resize(AppSession::MAX_RECENT_IMAGES);

                    for (size_t i = 0; i < AppSession::MAX_RECENT_IMAGES; i++)
                    {
                        auto& imageData     = session.RecentImages[i];
                        if (imageData.Empty())
                        {
                            recentImages[i] = nullptr;
                            continue;
                        }

                        recentImages[i]     = Image::Create(imageData.Data, imageData.Width, imageData.Height);
                    }
                }

                if (CustomButton("Procurar Imagem", { popupWidth / 2.0f, 120.0f }))
                {
                    std::string path;
                    if (FileDialog::Open({ { "Images", "*" } }, "", path) == FileDialogResult::SUCCESS)
                    {
                        const auto hash = HashString(path);
                        bank->Images[hash] = LoadImage(path);
                        question.Image = hash;
                        ImGui::CloseCurrentPopup();
                    }
                }

                ImGui::SameLine();

                ImGui::BeginDisabled(clipImage == nullptr);
                if (ImageButton("Área de Transferência", clipImage, { popupWidth / 2.0f, 120.0f }, clipImage == nullptr))
                {
                    auto handle = Application::Get().LoadClipboardImage(clipImage);
                    if (handle)
                    {
                        auto it = bank->Images.find(handle);
                        if (it != bank->Images.end())
                            question.Image = handle;
                    }
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndDisabled();

                ImGui::SeparatorText("Imagens Recentes");

                ImGui::BeginChild("##RecentImagesChild", { popupWidth, columnWidth + PADDING }, false);
                auto size               = ImGui::GetContentRegionAvail();

                const size_t imageCount = std::min(session.RecentImagesIndex, AppSession::MAX_RECENT_IMAGES);
                const int columns       = (int)AppSession::MAX_RECENT_IMAGES;
                int imageToRemove       = -1;

                if (ImGui::BeginTable("##RecentImages", columns, ImGuiTableFlags_SizingFixedSame))
                {
                    for (int i = 0; i < columns; i++)
                    {
                        ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthFixed, columnWidth);
                    }

                    ImGui::TableNextRow();

                    for (size_t column = 0; column < imageCount; ++column)
                    {
                        ImGui::TableSetColumnIndex(static_cast<int>(column));

                        auto& imageTexture = recentImages[column];
                        if (!imageTexture)
                            continue;

                        ImGui::PushID(static_cast<int>(column));

                        bool removeClicked         = false;
                        const bool selected        = ImageSelectButton(imageTexture, { previewSize, previewSize }, removeClicked);

                        if (selected)
                        {
                            auto curTime           = time(nullptr);
                            auto hash              = static_cast<uint64_t>(curTime);
                            HashCombine(hash, imageTexture->GetWidth(), imageTexture->GetHeight());

                            if (hash)
                            {
                                bank->Images[hash] = imageTexture;
                                question.Image     = hash;
                                Application::Get().PromoteRecentImage(column);
                            }

                            ImGui::PopID();

                            ImGui::CloseCurrentPopup();
                            break;
                        }

                        if (removeClicked)
                        {
                            imageToRemove         = static_cast<int>(column);
                        }

                        ImGui::PopID();
                    }

                    ImGui::EndTable();
                }


                if (imageToRemove >= 0)
                {
                    const size_t imageCount          = std::min(session.RecentImagesIndex, AppSession::MAX_RECENT_IMAGES);
                    if (imageToRemove < imageCount)
                    {
                        for (size_t i = imageToRemove; i + 1 < imageCount; ++i)
                        {
                            recentImages[i]          = recentImages[i + 1];
                        }

                        recentImages[imageCount - 1] = nullptr;
                    }

                    Application::Get().RemoveRecentImage(static_cast<size_t>(imageToRemove));
                }

                ImGui::EndChild();

                imageSelectorJustOpened             = false;
                ImGui::EndPopup();
            }

            ImDrawList* drawList  = ImGui::GetWindowDrawList();

            ImU32 backgroundColor = IM_COL32(30, 30, 30, 255);
            if (hovered)
                backgroundColor   = IM_COL32(45, 45, 45, 255);
            if (active)
                backgroundColor   = IM_COL32(60, 60, 60, 255);

            drawList->AddRectFilled(
                cursor,
                cursor + buttonSize,
                backgroundColor,
                4.0f
            );

            const float imageWidth  = static_cast<float>(image->GetWidth());
            const float imageHeight = static_cast<float>(image->GetHeight());

            if (imageWidth > 0.0f && imageHeight > 0.0f)
            {
                const float imageAspect    = imageWidth / imageHeight;
                const float viewportAspect = buttonSize.x / buttonSize.y;

                ImVec2 imageSize;
                if (viewportAspect > imageAspect)
                {
                    imageSize.y = buttonSize.y;
                    imageSize.x = imageSize.y * imageAspect;
                }
                else
                {
                    imageSize.x = buttonSize.x;
                    imageSize.y = imageSize.x / imageAspect;
                }

                const ImVec2 imagePos = {
                    cursor.x + (buttonSize.x - imageSize.x) * 0.5f,
                    cursor.y + (buttonSize.y - imageSize.y) * 0.5f
                };

                drawList->AddImage(
                    (ImTextureID)image->GetID(),
                    imagePos,
                    imagePos + imageSize
                );

                if (question.Image == 0)
                {
                    const std::string text = "Arraste ou selecione uma imagem!";

                    drawList->AddRectFilled(
                        imagePos,
                        imagePos + imageSize,
                        IM_COL32(0, 0, 0, hovered ? 120 : 90),
                        4.0f
                    );

                    const float lineHeight = ImGui::GetTextLineHeight();
                    const float y          = imagePos.y + (imageSize.y - lineHeight) * 0.5f;

                    const ImVec2 textSize  = ImGui::CalcTextSize(text.c_str());
                    const float x          = imagePos.x + (imageSize.x - textSize.x) * 0.5f;

                    drawList->AddText(
                        ImVec2(x, y),
                        IM_COL32(255, 255, 255, 255),
                        text.c_str()
                    );
                }
            }

            if (hovered)
            {
                drawList->AddRect(
                    cursor,
                    cursor + buttonSize,
                    active ? IM_COL32(255, 255, 255, 220) : IM_COL32(255, 255, 255, 150),
                    4.0f,
                    0,
                    active ? 2.0f : 1.0f
                );
            }
            else
            {
                drawList->AddRect(
                    cursor,
                    cursor + buttonSize,
                    IM_COL32(100, 100, 100, 180),
                    4.0f
                );
            }

            ImGui::PopID();
        }
        ImGui::EndGroup();

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(ExternalDragDrop::PayloadType))
            {
                const auto& files          = Application::Get().GetExternalDragDrop().GetPaths();

                for (const auto& file : files)
                {
                    const std::string path = file.string();
                    const auto hash        = HashString(path);

                    bank->Images[hash]     = LoadImage(path);
                    question.Image         = hash;

                    // One image is enough for this question.
                    break;
                }
            }

            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("IMAGES_ITEM"))
            {
                uint64_t handle        = *(uint64_t*)payload->Data;
                if (handle)
                {
                    auto it            = bank->Images.find(handle);
                    if (it != bank->Images.end())
                        question.Image = handle;
                }
            }

            ImGui::EndDragDropTarget();
        }

        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
            ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V))
        {
            if (!ImGui::GetIO().WantTextInput)
            {
                auto handle            = Application::Get().LoadClipboardImage();
                if (handle)
                {
                    auto it            = bank->Images.find(handle);
                    if (it != bank->Images.end())
                        question.Image = handle;
                }
            }
        }

        ImGui::Text("Texto da Questão:");
        ImGui::InputTextMultiline("##Text", questionText, sizeof(questionText), ImVec2(width, 50.0f), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_WordWrap);

        ImGui::Text("Explicação:");
        ImGui::InputTextMultiline("##Explanation", explanationText, sizeof(explanationText), ImVec2(width, 50.0f), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_WordWrap);

        const float lineCount  = static_cast<float>(std::max(CalculateCategoryLineCount(question.Categories, width), 1));
        const float textHeight = lineCount * ImGui::GetTextLineHeight() + (lineCount - 1.0f + 4.0f) * ImGui::GetStyle().ItemSpacing.y;

        ImGui::BeginChild("##NewQuestionCategories", { 0, textHeight }, true);
        DrawCategoryTexts(question.Categories, width, true);
        ImGui::EndChild();

        if (ImGui::Button("Adicionar Categoria"))
            ImGui::OpenPopup("CategorySelector");

        if (ImGui::BeginPopup("CategorySelector"))
        {
            static char searchBuffer[128] = {};
            static char newCategory[128] = {};

            ImGui::Text("Search:");
            ImGui::InputText("##Search", searchBuffer, sizeof(searchBuffer), ImGuiInputTextFlags_AutoSelectAll);

            auto toLower = [](std::string value)
            {
                std::transform(value.begin(), value.end(), value.begin(), [](unsigned char p_Char)
                {
                    return static_cast<char>(std::tolower(p_Char));
                });

                return value;
            };

            const std::string search = toLower(searchBuffer);
            auto hasCategory = [&](const std::string& category)
            {
                std::stringstream ss(question.Categories);
                std::string current;

                while (std::getline(ss, current, ';'))
                {
                    if (current == category)
                        return true;
                }

                return false;
            };

            auto addCategory = [&](const std::string& category)
            {
                if (category.empty())
                    return;

                std::stringstream ss(question.Categories);
                std::string current;

                while (std::getline(ss, current, ';'))
                {
                    if (current == category)
                        return;
                }

                if (!question.Categories.empty())
                    question.Categories += ';';

                question.Categories += category;
            };

            auto removeCategory = [&](const std::string& category)
            {
                std::stringstream ss(question.Categories);

                std::string current;
                std::string result;

                while (std::getline(ss, current, ';'))
                {
                    if (current == category)
                        continue;

                    if (!result.empty())
                        result += ';';

                    result += current;
                }

                question.Categories = std::move(result);
            };

            const auto size = ImGui::GetContentRegionAvail();
            ImGui::SetNextWindowSizeConstraints({ size.x, 50.0f }, { size.x, 150.0f });
            ImGui::BeginChild("##Categories", { 0, 0 }, true);
            {
                std::stringstream ss(bank->Categories);
                std::string category;

                while (std::getline(ss, category, ';'))
                {
                    if (!search.empty())
                    {
                        const std::string categoryLower = toLower(category);
                        if (categoryLower.find(search) == std::string::npos)
                        {
                            continue;
                        }
                    }

                    const bool selected = hasCategory(category);
                    if (ImGui::Selectable(category.c_str(), selected))
                    {
                        if (selected)
                            removeCategory(category);
                        else
                            addCategory(category);

                        searchBuffer[0] = '\0';
                        ImGui::CloseCurrentPopup();
                    }
                }
            }

            ImGui::EndChild();

            ImGui::BeginChild("##AddCategory", { 0, ImGui::GetFrameHeight() + 4 * ImGui::GetStyle().ItemSpacing.y }, true);

            auto createCategory = [&]()
            {
                std::string category(newCategory);

                if (category.empty())
                    return;

                // Add to bank only if it does not already exist.
                bool exists = false;

                {
                    std::stringstream ss(bank->Categories);
                    std::string current;

                    while (std::getline(ss, current, ';'))
                    {
                        if (current == category)
                        {
                            exists = true;
                            break;
                        }
                    }
                }

                if (!exists)
                {
                    if (!bank->Categories.empty())
                        bank->Categories += ';';

                    bank->Categories += category;
                }

                addCategory(category);

                newCategory[0] = '\0';
            };

            if (ImGui::InputText("##Category", newCategory, sizeof(newCategory), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
            {
                createCategory();
            }

            ImGui::SameLine();

            if (ImGui::Button(" + "))
                createCategory();

            ImGui::EndChild();

            ImGui::EndPopup();
        }

        ImGui::BeginChild("##Answers", { 0, 0 }, true);

        if (ImGui::InputText("##NewAnswer", answerText, sizeof(answerText), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
        {
            if (question.CurrentOptionIndex < static_cast<int>(question.Options.size()))
            {
                question.Options[question.CurrentOptionIndex] = answerText;
                question.CurrentOptionIndex++;
            }

            answerText[0] = '\0';
        }

        ImGui::SameLine();

        if (ImGui::Button(" + "))
        {
            if (question.CurrentOptionIndex < static_cast<int>(question.Options.size()))
            {
                question.Options[question.CurrentOptionIndex] = answerText;
                question.CurrentOptionIndex++;
            }

            answerText[0] = '\0';
        }

        std::vector<int> questionsToDelete;
        if (ImGui::BeginTable("AnswerOptions", 3, ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("A", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("B", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("C", ImGuiTableColumnFlags_WidthStretch);

            const int optionCount = std::clamp(question.CurrentOptionIndex, 0, static_cast<int>(question.Options.size()));
            for (int i = 0; i < optionCount; i += 3)
            {
                ImGui::TableNextRow();

                for (int column = 0; column < 3; column++)
                {
                    const int optionIndex = i + column;

                    ImGui::TableNextColumn();

                    if (optionIndex >= optionCount)
                        continue;

                    ImGui::PushID(optionIndex);

                    bool marked                         = question.CorrectOptionIndex == optionIndex;
                    if (ImGui::Checkbox("##Correct", &marked))
                    {
                        if (marked)
                        {
                            question.CorrectOptionIndex = optionIndex;
                            question.CorrectOptionText  = question.Options[optionIndex];
                        }
                        else
                        {
                            question.CorrectOptionIndex = -1;
                            question.CorrectOptionText.clear();
                        }
                    }

                    ImGui::SameLine();

                    if (marked)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, { 0.0f, 1.0f, 0.0f, 1.0f });
                    }

                    ImGui::Text("(%c)", 'A' + static_cast<char>(optionIndex));
                    ImGui::SameLine();

                    ImGui::PushTextWrapPos();
                    ImGui::TextUnformatted(question.Options[optionIndex].c_str());
                    ImGui::PopTextWrapPos();

                    ImGui::SameLine();

                    if (ImGui::SmallButton("X"))
                        questionsToDelete.push_back(optionIndex);

                    if (marked)
                        ImGui::PopStyleColor();

                    ImGui::PopID();
                }
            }

            ImGui::EndTable();
        }

        if (!questionsToDelete.empty())
        {
            std::sort(questionsToDelete.begin(), questionsToDelete.end());
            questionsToDelete.erase(std::unique(questionsToDelete.begin(), questionsToDelete.end()), questionsToDelete.end());

            const int oldCorrectIndex              = question.CorrectOptionIndex;
            const int oldCount                     = question.CurrentOptionIndex;

            std::array<std::string, 26> oldOptions = question.Options;

            question.Options.fill("");
            question.CurrentOptionIndex            = 0;
            question.CorrectOptionIndex            = -1;
            question.CorrectOptionText.clear();

            int newCorrectIndex                    = -1;
            for (int oldIndex = 0; oldIndex < oldCount; oldIndex++)
            {
                const bool deleted         = std::binary_search(questionsToDelete.begin(), questionsToDelete.end(), oldIndex);
                if (deleted)
                    continue;

                const int newIndex         = question.CurrentOptionIndex;
                question.Options[newIndex] = oldOptions[oldIndex];

                question.CurrentOptionIndex++;

                if (oldIndex == oldCorrectIndex)
                {
                    // The correct answer itself was deleted.
                    newCorrectIndex = -1;
                }
                else if (oldCorrectIndex >= 0 && oldIndex < oldCorrectIndex)
                {
                    // The correct answer moved one position left.
                    if (newCorrectIndex >= 0)
                        newCorrectIndex--;
                }
            }

            // Recalculate the correct index more robustly.
            if (oldCorrectIndex >= 0)
            {
                bool correctWasDeleted = std::binary_search(questionsToDelete.begin(), questionsToDelete.end(), oldCorrectIndex);
                if (!correctWasDeleted)
                {
                    int shift = 0;
                    for (int deletedIndex : questionsToDelete)
                    {
                        if (deletedIndex < oldCorrectIndex)
                            shift++;
                    }

                    newCorrectIndex   = oldCorrectIndex - shift;
                }
                else
                {
                    newCorrectIndex   = -1;
                }
            }

            question.CorrectOptionIndex    = newCorrectIndex;
            if (newCorrectIndex >= 0)
            {
                question.CorrectOptionText = question.Options[newCorrectIndex];
            }
        }

        ImGui::EndChild();

        ImGui::EndChild();

        if (ImGui::Button("Cancelar", ImVec2(100, 0)))
        {
            question           = {};

            questionText[0]    = '\0';
            explanationText[0] = '\0';
            answerText[0]      = '\0';

            m_QuestionToEdit   = -1;
            m_Active           = false;
        }

        ImGui::SameLine();

        const bool canSave =
            (questionText[0] != '\0' || question.Image > 0) &&
            question.CurrentOptionIndex > 0 &&
            question.CorrectOptionIndex >= 0 &&
            question.CorrectOptionIndex <
            question.CurrentOptionIndex;

        ImGui::BeginDisabled(!canSave);

        if (ImGui::Button("Salvar", ImVec2(100, 0)))
        {
            question.Text                   = questionText;
            question.Explanation            = explanationText;

            // Keep CorrectOptionText synchronized.
            if (question.CorrectOptionIndex >= 0 && question.CorrectOptionIndex < question.CurrentOptionIndex)
            {
                question.CorrectOptionText  = question.Options[question.CorrectOptionIndex];
            }
            else
            {
                question.CorrectOptionText.clear();
                question.CorrectOptionIndex = -1;
            }

            if (m_QuestionToEdit >= 0 && m_QuestionToEdit < static_cast<int>(bank->Questions.size()))
            {
                bank->Questions[m_QuestionToEdit] = question;
            }
            else
            {
                bank->Questions.push_back(question);
            }


            question           = {};

            questionText[0]    = '\0';
            explanationText[0] = '\0';
            answerText[0]      = '\0';

            m_QuestionToEdit   = -1;
            m_Active           = false;
        }

        ImGui::EndDisabled();

        ImGui::End();

        wasQuestionWindowOpen = m_Active;
    }

} // namespace QB
