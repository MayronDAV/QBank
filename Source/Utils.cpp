#include "Utils.h"
#include "Application.h"

// std
#include <iostream>
#include <sstream>

// lib
#include <imgui_internal.h>
#include <stb/stb_image.h>
#include <stb/stb_image_resize2.h>



namespace QB
{
    void DrawCategoryButtons(std::string& p_Categories, float p_Width, std::string& p_Selected, std::vector<int>& p_FilteredQuestions, bool p_IsTopBar, bool p_FilterIfNeeded)
    {
        auto bank = Application::Get().GetCurrentBank();
        if (!bank) return;

        std::stringstream ss(p_Categories);
        const ImGuiStyle& style = ImGui::GetStyle();
        const float spacing     = style.ItemSpacing.x; std::string category;
        float lineWidth         = 0.0f;

        auto hasCategory = [](const std::string& p_Categories, const std::string& p_Category)
        {
            std::stringstream ss(p_Categories);
            std::string current;

            while (std::getline(ss, current, ';'))
            {
                if (current == p_Category)
                    return true;
            }

            return false;
        };

        auto removeCategory = [](std::string& p_Categories, const std::string& p_Category)
        {
            std::stringstream ss(p_Categories);

            std::string current;
            std::string result;

            while (std::getline(ss, current, ';'))
            {
                if (current == p_Category)
                    continue;

                if (!result.empty())
                    result += ';';

                result += current;
            }

            p_Categories = std::move(result);
        };

        bool first = true;
        std::string categoryToDelete = "";
        while (std::getline(ss, category, ';'))
        {
            const ImVec2 textSize     = ImGui::CalcTextSize(category.c_str());
            const float buttonWidth   = textSize.x + style.FramePadding.x * 2.0f;
            const float requiredWidth = first ? buttonWidth : spacing + buttonWidth;
            if (!first && lineWidth + requiredWidth > p_Width)
                lineWidth = buttonWidth;
            else
            {
                if (!first)
                    ImGui::SameLine();
                lineWidth += requiredWidth;
            }

            bool selected = p_Selected.empty() ? false : p_Selected == category && p_IsTopBar;

            ImGui::PushID(category.c_str());

            auto activeColor = IM_COL32(60, 60, 60, 255);
            ImGui::PushStyleColor(ImGuiCol_Button, selected ? activeColor : IM_COL32(30, 30, 30, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(45, 45, 45, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeColor);

            if (ImGui::Button(category.c_str(), ImVec2(buttonWidth, 0.0f)) && p_FilterIfNeeded)
            {
                p_FilteredQuestions.clear();
                if (!selected && bank)
                {
                    for (size_t i = 0; i < bank->Questions.size(); i++)
                    {
                        auto& question = bank->Questions[i];
                        if (hasCategory(question.Categories, category))
                        {
                            p_FilteredQuestions.push_back((int)i);
                        }
                    }
                }
                p_Selected = selected ? "" : category;
            }

            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem("Deletar"))
                {
                    categoryToDelete = category;
                }

                ImGui::EndPopup();
            }

            ImGui::PopStyleColor(3);
            ImGui::PopID();
            first = false;
        }

        if (!categoryToDelete.empty())
        {
            if (p_IsTopBar)
            {
                if (p_Selected == categoryToDelete && p_FilterIfNeeded)
                {
                    p_Selected = "";
                    p_FilteredQuestions.clear();
                }

                for (auto& question : bank->Questions)
                {
                    if (hasCategory(question.Categories, categoryToDelete))
                    {
                        removeCategory(question.Categories, categoryToDelete);
                    }
                }
            }

            removeCategory(p_Categories, categoryToDelete);
        }
    }

    void DrawCategoryButtons(std::string& p_Categories, float p_Width, bool p_FilterIfNeeded)
    {
        std::string categorySelected = "";
        std::vector<int> filteredQuestions;
        DrawCategoryButtons(p_Categories, p_Width, categorySelected, filteredQuestions, false, p_FilterIfNeeded);
    }

    void DrawCategoryTexts(const std::string& p_Categories, float p_Width, bool p_AddSeparator)
    {
        std::stringstream ss(p_Categories);
        const ImGuiStyle& style = ImGui::GetStyle();
        const float spacing     = style.ItemSpacing.x; std::string category;
        float lineWidth         = 0.0f;

        bool first              = true;
        while (std::getline(ss, category, ';'))
        {
            const ImVec2 textSize     = ImGui::CalcTextSize(category.c_str());
            const float textWidth     = textSize.x /* + style.FramePadding.x * 2.0f */;
            const float requiredWidth = first ? textWidth : spacing + textWidth;
            if (!first && lineWidth + requiredWidth > p_Width)
                lineWidth             = textWidth;
            else
            {
                if (!first)
                    ImGui::SameLine();

                if (!first && p_AddSeparator)
                {
                    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                    ImGui::SameLine();
                }
                lineWidth += requiredWidth;
            }


            ImGui::Text(category.c_str());
            first = false;
        }
    }

    int CalculateCategoryLineCount(const std::string& p_Categories, float p_Width)
    {
        std::stringstream ss(p_Categories);
        const ImGuiStyle& style       = ImGui::GetStyle();
        const float spacing           = style.ItemSpacing.x;
        float lineWidth               = 0.0f;
        int lineCount                 = 1;
        bool first                    = true;
        std::string category;

        while (std::getline(ss, category, ';'))
        {
            if (category.empty())
                continue;

            const ImVec2 textSize     = ImGui::CalcTextSize(category.c_str());
            const float buttonWidth   = textSize.x + style.FramePadding.x * 2.0f;
            const float requiredWidth = first ? buttonWidth : spacing + buttonWidth;

            if (!first && lineWidth + requiredWidth > p_Width)
            {
                ++lineCount;
                lineWidth             = buttonWidth;
            }
            else
                lineWidth            += requiredWidth;
            first                     = false;
        }

        return first ? 0 : lineCount;
    }

    float CalculateCategoryButtonsHeight(const std::string& p_Categories, float p_Width)
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        int lineCount           = CalculateCategoryLineCount(p_Categories, p_Width);
        float height            = lineCount * ImGui::GetFrameHeight() + (lineCount - 1) * style.ItemSpacing.y;
        return height;
    }

    float CalculateWrappedTextHeight(const std::string& p_Text, float p_Width, float p_Padding)
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        const float textWidth   = p_Width - style.ChildBorderSize * 2.0f - p_Padding * 2.0f;
        const ImVec2 textSize   = ImGui::GetFont()->CalcTextSizeA( ImGui::GetFontSize(), FLT_MAX, textWidth, p_Text.c_str() );

        return textSize.y + p_Padding * 2.0f + style.ChildBorderSize * 2.0f;
    }


    uint8_t* LoadImageFromFile(const char* p_Path, uint32_t* p_Width, uint32_t* p_Height, uint32_t* p_Channels, uint32_t* p_Bytes, bool* p_IsHDR, bool p_FlipY)
    {
        stbi_set_flip_vertically_on_load(p_FlipY);

        int texWidth = 0, texHeight = 0, texChannels = 0;
        stbi_uc* pixels   = nullptr;
        int sizeOfChannel = 8;
        if (stbi_is_hdr(p_Path))
        {
            sizeOfChannel = 32;
            pixels        = (uint8_t*)stbi_loadf(p_Path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
            if (p_IsHDR)
                *p_IsHDR  = true;
        }
        else
        {
            pixels        = stbi_load(p_Path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
            if (p_IsHDR)
                *p_IsHDR  = false;
        }

        if (!p_IsHDR && s_MaxImageWidth > 0 && s_MaxImageHeight > 0 && ((uint32_t)texWidth > s_MaxImageWidth || (uint32_t)texHeight > s_MaxImageHeight))
        {
            uint32_t texWidthOld = texWidth, texHeightOld = texHeight;
            float aspectRatio    = static_cast<float>(texWidth) / static_cast<float>(texHeight);
            if ((uint32_t)texWidth > s_MaxImageWidth)
            {
                texWidth         = s_MaxImageWidth;
                texHeight        = static_cast<uint32_t>(s_MaxImageWidth / aspectRatio);
            }
            if ((uint32_t)texHeight > s_MaxImageHeight)
            {
                texHeight        = s_MaxImageHeight;
                texWidth         = static_cast<uint32_t>(s_MaxImageHeight * aspectRatio);
            }

            int resizedChannels    = texChannels;
            uint8_t* resizedPixels = (stbi_uc*)malloc(texWidth * texHeight * resizedChannels);

            if (p_IsHDR)
                stbir_resize_float_linear((float*)pixels, texWidthOld, texHeightOld, 0, (float*)resizedPixels, texWidth, texHeight, 0, STBIR_RGBA);
            else
                stbir_resize_uint8_linear(pixels, texWidthOld, texHeightOld, 0, resizedPixels, texWidth, texHeight, 0, STBIR_RGBA);

            free(pixels);
            pixels = resizedPixels;
        }

        if (!pixels)
        {
            std::cerr << "Could not load image '" << p_Path << "'!\n";

            texChannels = 4;

            if (p_Width)    *p_Width    = 2;
            if (p_Height)   *p_Height   = 2;
            if (p_Bytes)    *p_Bytes    = sizeOfChannel / 8;
            if (p_Channels) *p_Channels = texChannels;

            const int32_t size          = (*p_Width) * (*p_Height) * texChannels;
            uint8_t* data               = new uint8_t[size];

            uint8_t datatwo[16] = {
                255, 0  , 255, 255,
                0,   0  , 0,   255,
                0,   0  , 0,   255,
                255, 0  , 255, 255
            };

            memcpy(data, datatwo, size);

            return data;
        }

        if (texChannels != 4)
            texChannels = 4;

        if (p_Width)    *p_Width    = texWidth;
        if (p_Height)   *p_Height   = texHeight;
        if (p_Bytes)    *p_Bytes    = sizeOfChannel / 8;
        if (p_Channels) *p_Channels = texChannels;

        const uint64_t size         = uint64_t(texWidth) * uint64_t(texHeight) * uint64_t(texChannels) * uint64_t(sizeOfChannel / 8U);
        uint8_t* result             = new uint8_t[size];
        memcpy(result, pixels, size);

        stbi_image_free(pixels);
        return result;
    }

    std::shared_ptr<Image> LoadImage(const std::string& p_Path)
    {
        uint32_t width, height, channels = 4, bytes = 1;
        bool isHDR    = false;
        uint8_t* data = LoadImageFromFile(p_Path.c_str(), &width, &height, &channels, &bytes, &isHDR, false);

        ImageSpecification spec = {};
        spec.Width              = width;
        spec.Height             = height;
        spec.Format             = (isHDR) ? ImageFormat::RGBA32_FLOAT : ImageFormat::RGBA8;

        uint64_t imageSize      = uint64_t(width) * uint64_t(height) * uint64_t(channels) * uint64_t(bytes);
        auto texture            = Image::Create(spec, data, imageSize);
        if (!texture)
        {
            std::cerr << "Failed to create texture!\n";
            free(data);
            return nullptr;
        }

        free(data);
        return texture;
    }


    void AddText(ImDrawList* p_DrawList, const ImVec2& p_Pos, const ImVec2& p_Size, ImU32 p_Color, const char* p_Text, float p_Padding)
    {
        const float textWidth  = std::max(0.0f, p_Size.x - p_Padding * 2.0f);
        const float textHeight = std::max(0.0f, p_Size.y - p_Padding * 2.0f);

        const ImVec2 textSize = ImGui::CalcTextSize(p_Text, nullptr, false, textWidth);
        const ImVec2 textPos  = {
            p_Pos.x + (p_Size.x - textSize.x) * 0.5f,
            p_Pos.y + (p_Size.y - textSize.y) * 0.5f
        };

        p_DrawList->AddText(
            ImGui::GetFont(),
            ImGui::GetFontSize(),
            textPos,
            ImGui::GetColorU32(ImGuiCol_Text),
            p_Text,
            nullptr,
            textWidth
        );
    }

    void AddTextRotated(ImDrawList* p_DrawList, const ImVec2& p_Pos, ImU32 p_Color, const char* p_Text, float p_Angle)
    {
        const int vtxStart     = p_DrawList->VtxBuffer.Size;

        p_DrawList->AddText(p_Pos, p_Color, p_Text);

        const int vtxEnd       = p_DrawList->VtxBuffer.Size;

        const ImVec2 center    = {
            p_Pos.x + ImGui::CalcTextSize(p_Text).x * 0.5f,
            p_Pos.y + ImGui::GetFontSize() * 0.5f
        };

        const float s          = sinf(p_Angle);
        const float c          = cosf(p_Angle);

        for (int i = vtxStart; i < vtxEnd; i++)
        {
            ImDrawVert& vertex = p_DrawList->VtxBuffer[i];

            const ImVec2 p     = vertex.pos - center;
            vertex.pos.x       = p.x * c - p.y * s + center.x;
            vertex.pos.y       = p.x * s + p.y * c + center.y;
        }
    }

    bool VerticalButton(const char* p_Label, const ImVec2& p_Size, bool p_Selected)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        const bool pressed   = ImGui::InvisibleButton(p_Label, p_Size);
        const bool hovered   = ImGui::IsItemHovered();
        const bool active    = ImGui::IsItemActive();

        const ImVec2 min     = ImGui::GetItemRectMin();
        const ImVec2 max     = ImGui::GetItemRectMax();

        const float lineX    = min.x + 1.0f;

        ImU32 lineColor      = ImGui::GetColorU32(ImGuiCol_Border);
        if (active)
            lineColor        = ImGui::GetColorU32(ImGuiCol_ButtonActive);
        else if (p_Selected)
            lineColor        = ImGui::GetColorU32(ImGuiCol_ButtonActive);
        else if (hovered)
            lineColor        = ImGui::GetColorU32(ImGuiCol_ButtonHovered);

        const float lineThickness = (active || p_Selected) ? 3.0f : hovered ? 2.0f : 1.0f;
        drawList->AddLine(
            ImVec2(lineX, min.y),
            ImVec2(lineX, max.y),
            lineColor,
            lineThickness
        );

        const ImVec2 textSize = ImGui::CalcTextSize(p_Label);
        ImVec2 textPos        = {
            (min.x + max.x) * 0.5f - textSize.x * 0.5f,
            (min.y + max.y) * 0.5f - ImGui::GetFontSize() * 0.5f
        };

        AddTextRotated(
            drawList,
            textPos,
            ImGui::GetColorU32(ImGuiCol_Text),
            p_Label,
            IM_PI * 0.5f
        );

        return pressed;
    }

    bool ImageSelectButton(const std::shared_ptr<Image>& p_Image, const ImVec2& p_Size, bool& p_OutRemoveClicked, float p_Rounding, float p_ButtonSize, float p_ButtonPadding)
    {
        if (!p_Image) return false;

        const ImVec2 cursorPos  = ImGui::GetCursorScreenPos();
        ImDrawList* drawList    = ImGui::GetWindowDrawList();

        ImGui::InvisibleButton("##Image", p_Size, ImGuiButtonFlags_AllowOverlap);

        const bool hovered      = ImGui::IsItemHovered();
        const bool clicked      = ImGui::IsItemClicked(ImGuiMouseButton_Left);

        const float imageWidth  = static_cast<float>(p_Image->GetWidth());
        const float imageHeight = static_cast<float>(p_Image->GetHeight());

        ImVec2 imageSize        = p_Size;
        if (imageWidth > 0.0f && imageHeight > 0.0f)
        {
            const float scaleX  = p_Size.x / imageWidth;
            const float scaleY  = p_Size.y / imageHeight;
            const float scale   = std::min(scaleX, scaleY);

            imageSize.x         = imageWidth * scale;
            imageSize.y         = imageHeight * scale;
        }

        const ImVec2 imagePos   = {
            cursorPos.x + (p_Size.x - imageSize.x) * 0.5f,
            cursorPos.y + (p_Size.y - imageSize.y) * 0.5f
        };

        const ImVec2 imageEnd   = {
            imagePos.x + imageSize.x,
            imagePos.y + imageSize.y
        };

        drawList->AddRectFilled(
            cursorPos,
            cursorPos + p_Size,
            ImGui::GetColorU32(ImGuiCol_FrameBg),
            p_Rounding
        );

        drawList->AddImageRounded(
            (ImTextureID)p_Image->GetID(),
            imagePos,
            imageEnd,
            { 0.0f, 0.0f },
            { 1.0f, 1.0f },
            IM_COL32(255, 255, 255, 255),
            p_Rounding
        );

        drawList->AddRect(
            cursorPos,
            cursorPos + p_Size,
            ImGui::GetColorU32(
                hovered
                ? ImGuiCol_ButtonHovered
                : ImGuiCol_Border
            ),
            p_Rounding
        );

        if (hovered)
        {
            ImGui::PushID("RemoveButton");

            const ImVec2 buttonMin = {
                cursorPos.x + p_Size.x - p_ButtonSize,
                cursorPos.y
            };

            const ImVec2 buttonMax = {
                buttonMin.x + p_ButtonSize,
                buttonMin.y + p_ButtonSize
            };

            ImGui::SetCursorScreenPos(buttonMin);

            ImGui::InvisibleButton("##Remove", { p_ButtonSize, p_ButtonSize }, ImGuiButtonFlags_None);

            const bool removeHovered = ImGui::IsItemHovered();
            p_OutRemoveClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

            drawList->AddRectFilled(
                buttonMin,
                buttonMax,
                ImGui::GetColorU32(
                    removeHovered
                    ? ImGuiCol_ButtonHovered
                    : ImGuiCol_Button
                ),
                3.0f
            );

            const ImVec2 center = {
                buttonMin.x + p_ButtonSize * 0.5f,
                buttonMin.y + p_ButtonSize * 0.5f
            };

            const float halfSize = (p_ButtonSize - p_ButtonPadding * 2.0f) * 0.5f;
            const ImU32 color    = ImGui::GetColorU32(ImGuiCol_Text);

            drawList->AddLine(
                { center.x - halfSize, center.y - halfSize },
                { center.x + halfSize, center.y + halfSize },
                color,
                1.5f
            );

            drawList->AddLine(
                { center.x + halfSize, center.y - halfSize },
                { center.x - halfSize, center.y + halfSize },
                color,
                1.5f
            );

            ImGui::PopID();
        }

        return clicked && !p_OutRemoveClicked;
    }

    bool CustomButton(const char* p_Text, const ImVec2& p_Size, float p_Rounding, float p_TextPadding)
    {
        ImGui::PushID(p_Text);

        const ImVec2 cursor   = ImGui::GetCursorScreenPos();
        ImDrawList* drawList  = ImGui::GetWindowDrawList();

        const bool clicked    = ImGui::InvisibleButton("##CustomButton", p_Size);
        const bool hovered    = ImGui::IsItemHovered();
        const bool active     = ImGui::IsItemActive();

        ImU32 backgroundColor = ImGui::GetColorU32(ImGuiCol_Button);
        if (hovered)
            backgroundColor   = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
        if (active)
            backgroundColor   = ImGui::GetColorU32(ImGuiCol_ButtonActive);

        drawList->AddRectFilled(
            cursor,
            cursor + p_Size,
            backgroundColor,
            p_Rounding
        );

        if (hovered)
        {
            drawList->AddRectFilled(
                cursor,
                cursor + p_Size,
                IM_COL32(0, 0, 0, active ? 35 : 20),
                p_Rounding
            );
        }

        AddText(
            drawList,
            cursor,
            p_Size,
            ImGui::GetColorU32(ImGuiCol_Text),
            p_Text,
            p_TextPadding
        );

        auto borderColor = ImGui::GetStyleColorVec4(ImGuiCol_Border);
        borderColor.z    = 0.8f;

        drawList->AddRect(
            cursor,
            cursor + p_Size,
            ImGui::GetColorU32(borderColor),
            p_Rounding,
            0,
            active ? 2.0f : hovered ? 1.5f : 1.0f
        );

        ImGui::PopID();

        return clicked;
    }

    bool ImageButton(const char* p_Text, const std::shared_ptr<Image>& p_Image, const ImVec2& p_Size, bool p_AddText, float p_Rounding, float p_TextPadding)
    {
        ImGui::PushID(p_Text);

        const ImVec2 cursor   = ImGui::GetCursorScreenPos();
        ImDrawList* drawList  = ImGui::GetWindowDrawList();

        const bool clicked    = ImGui::InvisibleButton("##CustomButton", p_Size);
        const bool hovered    = ImGui::IsItemHovered();
        const bool active     = ImGui::IsItemActive();

        ImU32 backgroundColor = ImGui::GetColorU32(ImGuiCol_Button);
        if (hovered)
            backgroundColor   = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
        if (active)
            backgroundColor   = ImGui::GetColorU32(ImGuiCol_ButtonActive);

        drawList->AddRectFilled(
            cursor,
            cursor + p_Size,
            backgroundColor,
            p_Rounding
        );

        if (hovered)
        {
            drawList->AddRectFilled(
                cursor,
                cursor + p_Size,
                IM_COL32(0, 0, 0, active ? 35 : 20),
                p_Rounding
            );
        }

        if (p_Image)
        {
            const float imageWidth     = static_cast<float>(p_Image->GetWidth());
            const float imageHeight    = static_cast<float>(p_Image->GetHeight());
            const float imageAspect    = imageWidth / imageHeight;
            const float viewportAspect = p_Size.x / p_Size.y;

            ImVec2 imageSize;
            if (viewportAspect > imageAspect)
            {
                imageSize.y = p_Size.y;
                imageSize.x = imageSize.y * imageAspect;
            }
            else
            {
                imageSize.x = p_Size.x;
                imageSize.y = imageSize.x / imageAspect;
            }

            const ImVec2 imagePos = {
                cursor.x + (p_Size.x - imageSize.x) * 0.5f,
                cursor.y + (p_Size.y - imageSize.y) * 0.5f
            };

            drawList->AddImage(
                (ImTextureID)p_Image->GetID(),
                imagePos,
                imagePos + imageSize
            );
        }

        if (p_AddText)
        {
            AddText(
                drawList,
                cursor,
                p_Size,
                ImGui::GetColorU32(ImGuiCol_Text),
                p_Text,
                p_TextPadding
            );
        }

        auto borderColor = ImGui::GetStyleColorVec4(ImGuiCol_Border);
        borderColor.z    = 0.8f;

        drawList->AddRect(
            cursor,
            cursor + p_Size,
            ImGui::GetColorU32(borderColor),
            p_Rounding,
            0,
            active ? 2.0f : hovered ? 1.5f : 1.0f
        );

        ImGui::PopID();

        return clicked;
    }

    bool CustomMenuButton(const char* p_Label, const ImVec2& p_Size, bool p_Active, bool p_Enabled)
    {
        ImGui::PushID(p_Label);

        ImGui::BeginDisabled(!p_Enabled);

        const ImVec2 cursor  = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        const bool pressed   = ImGui::InvisibleButton("##MenuButton", p_Size);
        const bool hovered   = ImGui::IsItemHovered();
        const bool active    = ImGui::IsItemActive();

        AddText(
            drawList,
            cursor,
            p_Size,
            ImGui::GetColorU32(ImGuiCol_Text),
            p_Label,
            0.0f
        );

        ImU32 lineColor = ImGui::GetColorU32(ImGuiCol_Button);
        if (active || p_Active)
            lineColor   = ImGui::GetColorU32(ImGuiCol_ButtonActive);
        else if (hovered)
            lineColor   = ImGui::GetColorU32(ImGuiCol_ButtonHovered);

        constexpr float lineHeight = 2.0f;

        drawList->AddRectFilled(
            { cursor.x, cursor.y + p_Size.y - lineHeight },
            { cursor.x + p_Size.x, cursor.y + p_Size.y },
            lineColor
        );

        ImGui::PopID();

        ImGui::EndDisabled();
        return pressed;
    }

    std::string GetCustomMenuID(const char* p_Label)
    {
        return std::string("##CustomMenu_") + p_Label;
    }

    bool CustomBeginMenu(const char* p_Label, const ImVec2& p_Size, bool p_Enabled)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();

        if (window->SkipItems)
            return false;

        const std::string popupID = GetCustomMenuID(p_Label);

        const ImGuiID popupHash = window->GetID(
            popupID.c_str()
        );

        const bool popupOpen = ImGui::IsPopupOpen(
            popupHash,
            ImGuiPopupFlags_None
        );

        // ---------------------------------------------------------
        // Button position
        // ---------------------------------------------------------

        const ImVec2 buttonPos = ImGui::GetCursorScreenPos();

        // ---------------------------------------------------------
        // Custom button
        // ---------------------------------------------------------

        const bool pressed = CustomMenuButton(
            p_Label,
            p_Size,
            popupOpen,
            p_Enabled
        );

        if (!p_Enabled)
        {
            if (popupOpen)
                ImGui::ClosePopupToLevel(
                    ImGui::GetCurrentWindow()->BeginOrderWithinContext,
                    false);

            return false;
        }

        // ---------------------------------------------------------
        // Open / close
        // ---------------------------------------------------------

        if (pressed)
        {
            if (popupOpen)
            {
                ImGui::ClosePopupToLevel(
                    ImGui::GetCurrentWindow()->BeginOrderWithinContext,
                    false);
            }
            else
            {
                ImGui::OpenPopup(
                    popupID.c_str()
                );
            }
        }

        if (!ImGui::IsPopupOpen(
            popupHash,
            ImGuiPopupFlags_None))
        {
            return false;
        }

        // ---------------------------------------------------------
        // Popup position
        // ---------------------------------------------------------

        ImGui::SetNextWindowPos(
            ImVec2(
                buttonPos.x,
                buttonPos.y + p_Size.y
            ),
            ImGuiCond_Always
        );

        // ---------------------------------------------------------
        // Popup style
        // ---------------------------------------------------------

        ImGui::PushStyleVar(
            ImGuiStyleVar_WindowRounding,
            ImGui::GetStyle().PopupRounding
        );

        ImGui::PushStyleVar(
            ImGuiStyleVar_WindowPadding,
            ImGui::GetStyle().WindowPadding
        );

        constexpr ImGuiWindowFlags popupFlags =
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoNavFocus;

        const bool opened = ImGui::BeginPopup(
            popupID.c_str(),
            popupFlags
        );

        ImGui::PopStyleVar(2);

        return opened;
    }

    void CustomEndMenu()
    {
        ImGui::EndPopup();
    }

} // namespace QB
