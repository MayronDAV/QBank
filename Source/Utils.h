#pragma once
#include "Image.h"

// lib
#include <imgui.h>

// std
#include <string>
#include <memory>



namespace QB
{
    inline constexpr float PADDING               = 10.0f;
    inline constexpr const char* FOLDER_PATH     = "Banks";
    inline constexpr uint32_t s_MaxImageWidth    = 2048;
    inline constexpr uint32_t s_MaxImageHeight   = 2048;
    inline constexpr float QUESTION_IMAGE_HEIGHT = 100.0f;
    inline constexpr float SIDEBAR_WIDTH         = 40.0f;

    inline uint64_t HashString(std::string_view p_Str)
    {
        uint64_t hash = 14695981039346656037ull;

        for (char c : p_Str)
        {
            hash ^= (uint64_t)c;
            hash *= 1099511628211ull;
        }

        return hash;
    }

    template <typename T>
    inline void     HashCombine(std::size_t& p_Seed, const T& p_Value)
    {
        std::hash<T> hasher;
        p_Seed ^= hasher(p_Value) + 0x9e3779b9 + (p_Seed << 6) + (p_Seed >> 2);
    }

    template <typename T, typename... Rest>
    inline void     HashCombine(std::size_t& p_Seed, const T& p_Value, const Rest&... p_Rest)
    {
        HashCombine(p_Seed, p_Value);
        (HashCombine(p_Seed, p_Rest), ...);
    }

    template <class T, class U>
    inline std::shared_ptr<U> As(const std::shared_ptr<T>& p_Class)
    {
        static_assert(std::is_convertible_v<U*, T*> || std::is_base_of_v<U, T>,
            "Invalid cast: T must be convertible to U or U must be a base of T.");
        return std::dynamic_pointer_cast<U>(p_Class);
    }

    void  DrawCategoryButtons(std::string& p_Categories, float p_Width, std::string& p_Selected, std::vector<int>& p_FilteredQuestions, bool p_IsTopBar = false, bool p_FilterIfNeeded = true);
    void  DrawCategoryButtons(std::string& p_Categories, float p_Width, bool p_FilterIfNeeded = true);
    void  DrawCategoryTexts(const std::string& p_Categories, float p_Width, bool p_AddSeparator = false);
    int   CalculateCategoryLineCount(const std::string& p_Categories, float p_Width);
    float CalculateCategoryButtonsHeight(const std::string& p_Categories, float p_Width);
    float CalculateWrappedTextHeight(const std::string& p_Text, float p_Width, float p_Padding = PADDING);

    uint8_t* LoadImageFromFile(const char* p_Path, uint32_t* p_Width, uint32_t* p_Height, uint32_t* p_Channels, uint32_t* p_Bytes, bool* p_IsHDR, bool p_FlipY);
    std::shared_ptr<Image> LoadImage(const std::string& p_Path);

    void  AddText(ImDrawList* p_DrawList, const ImVec2& p_Pos, const ImVec2& p_Size, ImU32 p_Color, const char* p_Text, float p_Padding = PADDING);
    void  AddTextRotated(ImDrawList* p_DrawList, const ImVec2& p_Pos, ImU32 p_Color, const char* p_Text, float p_Angle);
    bool  VerticalButton(const char* p_Label, const ImVec2& p_Size, bool p_Selected = false);
    bool  ImageSelectButton(const std::shared_ptr<Image>& p_Image, const ImVec2& p_Size, bool& p_OutRemoveClicked, float p_Rounding = 4.0f, float p_ButtonSize = 20.0f, float p_ButtonPadding = 4.0f);
    bool  CustomButton(const char* p_Text, const ImVec2& p_Size, float p_Rounding = 4.0f, float p_TextPadding = 8.0f);
    bool  ImageButton(const char* p_Text, const std::shared_ptr<Image>& p_Image, const ImVec2& p_Size, bool p_AddText = false, float p_Rounding = 4.0f, float p_TextPadding = 8.0f);
    bool  CustomMenuButton(const char* p_Label, const ImVec2& p_Size, bool p_Active = false, bool p_Enabled = true);
    
    std::string GetCustomMenuID(const char* p_Label);
    bool  CustomBeginMenu(const char* p_Label, const ImVec2& p_Size, bool p_Enabled = true);
    void  CustomEndMenu();

} // namespace QB