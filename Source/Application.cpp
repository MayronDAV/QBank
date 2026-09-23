#include "Application.h"
#include "FileDialog.h"
#include "Clipboard.h"

// std
#include <iostream>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <ctime>

// lib
#include <glad/glad.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <stb/stb_image.h>
#include <stb/stb_image_resize2.h>


// TODO: Add an image compressor

namespace QB
{
    namespace
    {
        static constexpr float PADDING               = 10.0f;
        static constexpr const char* FOLDER_PATH     = "Banks";
        static uint32_t s_MaxImageWidth              = 2048;
        static uint32_t s_MaxImageHeight             = 2048;
        static constexpr float QUESTION_IMAGE_HEIGHT = 100.0f;
        static constexpr float SIDEBAR_WIDTH         = 40.0f;

        static void  BeginDockspace(std::string p_ID, std::string p_Dockspace, bool p_MenuBar, ImGuiDockNodeFlags p_DockFlags, ImGuiWindowFlags p_WindowFlags = 0)
        {
            static bool opt_fullscreen = true;
            static bool opt_padding = false;
            static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_NoWindowMenuButton | ImGuiDockNodeFlags_NoCloseButton;

            ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | p_WindowFlags;
            if (p_MenuBar)
                window_flags |= ImGuiWindowFlags_MenuBar;

            if (opt_fullscreen)
            {
                const ImGuiViewport* viewport = ImGui::GetMainViewport();
                ImGui::SetNextWindowPos(viewport->WorkPos);
                ImGui::SetNextWindowSize(viewport->WorkSize);
                ImGui::SetNextWindowViewport(viewport->ID);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
                window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
                    | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
                window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
            }
            else
            {
                dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
            }

            if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
                window_flags |= ImGuiWindowFlags_NoBackground;

            ImGui::PushStyleColor(ImGuiCol_WindowBg, { 0.0f, 0.0f, 0.0f, 1.0f });
            if (!opt_padding)
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            ImGui::Begin(p_Dockspace.c_str(), nullptr, window_flags);
            if (!opt_padding)
                ImGui::PopStyleVar();
            ImGui::PopStyleColor(); // windowBg

            if (opt_fullscreen)
                ImGui::PopStyleVar(2);

            // Submit the DockSpace
            ImGuiIO& io = ImGui::GetIO();
            if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
            {
                ImGuiID dockspace_id = ImGui::GetID(p_ID.c_str());
                ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags | p_DockFlags);
            }
        }

        static void  EndDockspace()
        {
            ImGui::End();
        }

        static void  DrawCategoryTexts(const std::string& categories, float p_Width, bool p_AddSeparator = false)
        {
            std::stringstream ss(categories);
            const ImGuiStyle& style = ImGui::GetStyle();
            const float spacing = style.ItemSpacing.x; std::string category;
            float lineWidth = 0.0f;
            bool first = true;

            while (std::getline(ss, category, ';'))
            {
                const ImVec2 textSize = ImGui::CalcTextSize(category.c_str());
                const float textWidth = textSize.x /* + style.FramePadding.x * 2.0f */;
                const float requiredWidth = first ? textWidth : spacing + textWidth;
                if (!first && lineWidth + requiredWidth > p_Width)
                    lineWidth = textWidth;
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

        static int   CalculateCategoryLineCount(const std::string& p_Categories, float p_Width)
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

        static float CalculateCategoryButtonsHeight(const std::string& p_Categories, float p_Width)
        {
            const ImGuiStyle& style = ImGui::GetStyle();
            int lineCount           = CalculateCategoryLineCount(p_Categories, p_Width);
            float height            = lineCount * ImGui::GetFrameHeight() + (lineCount - 1) * style.ItemSpacing.y;
            return height;
        }

        static float CalculateWrappedTextHeight(const std::string& p_Text, float p_Width, float p_Padding = PADDING)
        {
            const ImGuiStyle& style = ImGui::GetStyle();
            const float textWidth   = p_Width - style.ChildBorderSize * 2.0f - p_Padding * 2.0f;
            const ImVec2 textSize   = ImGui::GetFont()->CalcTextSizeA( ImGui::GetFontSize(), FLT_MAX, textWidth, p_Text.c_str() );

            return textSize.y + p_Padding * 2.0f + style.ChildBorderSize * 2.0f;
        }

        static float CalculateQuestionDetailsHeight(const Question& p_Question, float p_Width)
        {
            const ImGuiStyle& style         = ImGui::GetStyle();
            const float childBorder         = style.ChildBorderSize;
            const float itemSpacingY        = style.ItemSpacing.y;
            const float questionInfoHeight  = CalculateWrappedTextHeight(p_Question.Text, p_Width);
            float categoriesHeight          = CalculateCategoryButtonsHeight(p_Question.Categories, p_Width);

            const int optionCount           = p_Question.CurrentOptionIndex + 1;
            const int rowCount              = (optionCount + 2) / 3;
            float answerTableHeight         = 0.0f;
            if (rowCount > 0)
            {
                const float rowHeight       = ImGui::GetFrameHeight();
                answerTableHeight           = rowCount * rowHeight + (rowCount - 1) * style.CellPadding.y * 2.0f;
            }

            const float contentHeight       = categoriesHeight + itemSpacingY + questionInfoHeight + itemSpacingY + answerTableHeight;

            return contentHeight + style.WindowPadding.y * 2.0f + childBorder * 2.0f + PADDING;
        }

        static void  WriteString(std::ofstream& p_Out, const std::string& p_Str)
        {
            size_t size = p_Str.size();
            p_Out.write(reinterpret_cast<const char*>(&size), sizeof(size));
            p_Out.write(p_Str.data(), size);
        }

        static std::string ReadString(std::ifstream& p_In)
        {
            size_t size = 0;
            p_In.read(reinterpret_cast<char*>(&size), sizeof(size));

            std::string str(size, '\0');
            p_In.read(&str[0], size);

            return str;
        }

        struct QBHeader
        {
            char Magic[5]         = { 'Q', 'B', 'A', 'N', 'K'};
            uint32_t Version      = 1;
            size_t TotalQuestions = 0;
            size_t TotalImages    = 0;
        };

        static bool  ExportQuestionBank(const Bank& p_Bank)
        {
            auto filepath = std::filesystem::absolute(p_Bank.Path).replace_extension(".qbank");
            if (!std::filesystem::exists(filepath.parent_path()))
                std::filesystem::create_directories(filepath.parent_path());

            std::ofstream out(filepath, std::ios::binary | std::ios::trunc);
            if (!out)
            {
                std::cerr << "Failed to export bank at " << filepath.string() << "\n";
                return false;
            }

            QBHeader header       = {};
            header.TotalQuestions = p_Bank.Questions.size();
            header.TotalImages    = p_Bank.Images.size();
            out.write(reinterpret_cast<const char*>(&header), sizeof(header));

            WriteString(out, p_Bank.Categories);

            for (size_t i = 0; i < header.TotalQuestions; i++)
            {
                auto& question = p_Bank.Questions[i];
                WriteString(out, question.Text);
                WriteString(out, question.Explanation);
                WriteString(out, question.Categories);
                WriteString(out, question.CorrectOptionText);
                out.write(reinterpret_cast<const char*>(&question.Image), sizeof(question.Image));
                out.write(reinterpret_cast<const char*>(&question.CorrectOptionIndex), sizeof(question.CorrectOptionIndex));
                out.write(reinterpret_cast<const char*>(&question.CurrentOptionIndex), sizeof(question.CurrentOptionIndex));

                for (int j = 0; j < question.CurrentOptionIndex; j++)
                {
                    WriteString(out, question.Options[j]);
                }
            }

            for (auto& [id, image] : p_Bank.Images)
            {
                out.write(reinterpret_cast<const char*>(&id), sizeof(id));

                bool hasData     = image != nullptr;
                out.write(reinterpret_cast<const char*>(&hasData), sizeof(hasData));
                if (!hasData) continue;

                auto spec        = image->GetSpecification();

                out.write(reinterpret_cast<const char*>(&spec.Width),     sizeof(spec.Width));
                out.write(reinterpret_cast<const char*>(&spec.Height),    sizeof(spec.Height));
                out.write(reinterpret_cast<const char*>(&spec.Format),    sizeof(spec.Format));
                out.write(reinterpret_cast<const char*>(&spec.MinFilter), sizeof(spec.MinFilter));
                out.write(reinterpret_cast<const char*>(&spec.MagFilter), sizeof(spec.MagFilter));
                out.write(reinterpret_cast<const char*>(&spec.WrapU),     sizeof(spec.WrapU));
                out.write(reinterpret_cast<const char*>(&spec.WrapV),     sizeof(spec.WrapV));

                auto textureData = image->GetData();
                size_t dataSize  = textureData.size();

                out.write(reinterpret_cast<const char*>(&dataSize), sizeof(dataSize));
                out.write(reinterpret_cast<const char*>(textureData.data()), dataSize);
            }

            return true;
        }

        static Bank  ImportQuestionBank(const std::string& p_Path)
        {
            Bank bank = {};
            bank.Path = std::filesystem::absolute(p_Path).string();

            std::ifstream in(p_Path, std::ios::binary);
            if (!in)
            {
                std::cerr << "Failed to import bank: " << p_Path << "\n";
                return {};
            }

            QBHeader header;
            in.read(reinterpret_cast<char*>(&header), sizeof(header));

            bank.Categories                = ReadString(in);

            for (size_t i = 0; i < header.TotalQuestions; i++)
            {
                auto& question             = bank.Questions.emplace_back();
                question.Text              = ReadString(in);
                question.Explanation       = ReadString(in);
                question.Categories        = ReadString(in);
                question.CorrectOptionText = ReadString(in);
                in.read(reinterpret_cast<char*>(&question.Image), sizeof(question.Image));
                in.read(reinterpret_cast<char*>(&question.CorrectOptionIndex), sizeof(question.CorrectOptionIndex));
                in.read(reinterpret_cast<char*>(&question.CurrentOptionIndex), sizeof(question.CurrentOptionIndex));

                for (int j = 0; j < question.CurrentOptionIndex; j++)
                {
                    question.Options[j]    = ReadString(in);

                    if (question.CorrectOptionText == question.Options[j])
                        question.CorrectOptionIndex = j;
                }
            }

            for (size_t i = 0; i < header.TotalImages; i++)
            {
                uint64_t id = 0;
                in.read(reinterpret_cast<char*>(&id), sizeof(id));

                bool hasData = false;
                in.read(reinterpret_cast<char*>(&hasData), sizeof(hasData));
                if (!hasData) continue;

                ImageSpecification spec = {};
                in.read(reinterpret_cast<char*>(&spec.Width), sizeof(spec.Width));
                in.read(reinterpret_cast<char*>(&spec.Height), sizeof(spec.Height));
                in.read(reinterpret_cast<char*>(&spec.Format), sizeof(spec.Format));
                in.read(reinterpret_cast<char*>(&spec.MinFilter), sizeof(spec.MinFilter));
                in.read(reinterpret_cast<char*>(&spec.MagFilter), sizeof(spec.MagFilter));
                in.read(reinterpret_cast<char*>(&spec.WrapU), sizeof(spec.WrapU));
                in.read(reinterpret_cast<char*>(&spec.WrapV), sizeof(spec.WrapV));

                size_t dataSize = 0;
                in.read(reinterpret_cast<char*>(&dataSize), sizeof(dataSize));

                std::vector<uint8_t> textureData(dataSize);
                in.read(reinterpret_cast<char*>(textureData.data()), dataSize);

                bank.Images[id] = Image::Create(spec, textureData.data(), dataSize);
            }

            return bank;
        }

        struct APPHeader
        {
            char Magic[5]         = { 'Q', 'B', 'A', 'P', 'P' };
            uint32_t Version      = 2;
            size_t BanksCount     = 0;
        };

        static bool  ExportAppSession(const AppSession& p_Session)
        {
            const std::string path = "session.qbapp";
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            if (!out)
            {
                std::cerr << "Failed to export app config at " << path << "\n";
                return false;
            }

            APPHeader header;
            header.BanksCount = p_Session.Banks.size();
            out.write(reinterpret_cast<const char*>(&header), sizeof(header));

            out.write(reinterpret_cast<const char*>(&p_Session.Maximize), sizeof(p_Session.Maximize));
            out.write(reinterpret_cast<const char*>(&p_Session.Width), sizeof(p_Session.Width));
            out.write(reinterpret_cast<const char*>(&p_Session.Height), sizeof(p_Session.Height));

            WriteString(out, p_Session.StartBank);
            WriteString(out, p_Session.CategorySelected);

            for (size_t i = 0; i < header.BanksCount; i++)
            {
                WriteString(out, p_Session.Banks[i]);
            }

            out.write(reinterpret_cast<const char*>(&p_Session.RecentImagesIndex), sizeof(p_Session.RecentImagesIndex));
            for (size_t i = 0; i < AppSession::MAX_RECENT_IMAGES; i++)
            {
                auto& image = p_Session.RecentImages[i];

                bool hasData = !image.Empty();
                out.write(reinterpret_cast<const char*>(&hasData), sizeof(hasData));
                if (!hasData) continue;

                out.write(reinterpret_cast<const char*>(&image.Width), sizeof(image.Width));
                out.write(reinterpret_cast<const char*>(&image.Height), sizeof(image.Height));

                size_t dataSize = image.Data.size();
                out.write(reinterpret_cast<const char*>(&dataSize), sizeof(dataSize));
                out.write(reinterpret_cast<const char*>(image.Data.data()), dataSize);
            }

            return true;
        }

        static AppSession ImportAppSession()
        {
            const std::string path   = "session.qbapp";
            AppSession session = {};

            if (!std::filesystem::exists(path))
                return session;

            std::ifstream in(path, std::ios::binary);
            if (!in)
            {
                std::cerr << "Failed to import app session: " << path << "\n";
                return session;
            }

            APPHeader header;
            in.read(reinterpret_cast<char*>(&header), sizeof(header));
            if (header.Version > 2)
            {
                std::cerr << "Failed to import app session: " << path << ", the session is more up-to-date!\n";
                return {};
            }

            in.read(reinterpret_cast<char*>(&session.Maximize), sizeof(session.Maximize));
            in.read(reinterpret_cast<char*>(&session.Width), sizeof(session.Width));
            in.read(reinterpret_cast<char*>(&session.Height), sizeof(session.Height));

            session.StartBank        = ReadString(in);
            session.CategorySelected = ReadString(in);

            session.Banks.resize(header.BanksCount);
            for (size_t i = 0; i < header.BanksCount; i++)
            {
                session.Banks[i] = ReadString(in);
            }

            session.RecentImagesIndex = 0;
            session.RecentImages.fill({});

            if (header.Version >= 2)
            {
                in.read(reinterpret_cast<char*>(&session.RecentImagesIndex), sizeof(session.RecentImagesIndex));
                for (size_t i = 0; i < AppSession::MAX_RECENT_IMAGES; i++)
                {
                    bool hasData = false;
                    in.read(reinterpret_cast<char*>(&hasData), sizeof(hasData));
                    if (!hasData) continue;

                    ImageData image;

                    in.read(reinterpret_cast<char*>(&image.Width), sizeof(image.Width));
                    in.read(reinterpret_cast<char*>(&image.Height), sizeof(image.Height));

                    size_t dataSize = 0;
                    in.read(reinterpret_cast<char*>(&dataSize), sizeof(dataSize));

                    image.Data.resize(dataSize);
                    in.read(reinterpret_cast<char*>(image.Data.data()), dataSize);

                    session.RecentImages[i] = image;
                }
            }

            return session;
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

        static uint64_t HashString(std::string_view p_Str)
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
        static void HashCombine(std::size_t& p_Seed, const T& p_Value)
        {
            std::hash<T> hasher;
            p_Seed ^= hasher(p_Value) + 0x9e3779b9 + (p_Seed << 6) + (p_Seed >> 2);
        }

        template <typename T, typename... Rest>
        static void HashCombine(std::size_t& p_Seed, const T& p_Value, const Rest&... p_Rest)
        {
            HashCombine(p_Seed, p_Value);
            (HashCombine(p_Seed, p_Rest), ...);
        }

        static void AddTextRotated(ImDrawList* p_DrawList, const ImVec2& p_Pos, ImU32 p_Color, const char* p_Text, float p_Angle)
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

        static bool VerticalButton(const char* p_Label, const ImVec2& p_Size, bool p_Selected = false)
        {
            const bool pressed        = ImGui::InvisibleButton(p_Label, p_Size);
            const bool hovered        = ImGui::IsItemHovered();
            const bool active         = ImGui::IsItemActive();

            ImDrawList* drawList      = ImGui::GetWindowDrawList();

            const ImVec2 min          = ImGui::GetItemRectMin();
            const ImVec2 max          = ImGui::GetItemRectMax();

            const float lineX         = min.x + 1.0f;

            ImU32 lineColor;
            if (active)
                lineColor             = ImGui::GetColorU32(ImGuiCol_ButtonActive);
            else if (p_Selected)
                lineColor             = ImGui::GetColorU32(ImGuiCol_ButtonActive);
            else if (hovered)
                lineColor             = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
            else
                lineColor             = ImGui::GetColorU32(ImGuiCol_Border);

            const float lineThickness = (active || p_Selected) ? 3.0f : hovered ? 2.0f : 1.0f;
            drawList->AddLine(
                ImVec2(lineX, min.y),
                ImVec2(lineX, max.y),
                lineColor,
                lineThickness
            );

            const ImVec2 textSize     = ImGui::CalcTextSize(p_Label);
            ImVec2 textPos            = {
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

        static bool ImageSelectButton(const std::shared_ptr<Image>& p_Image, const ImVec2& p_Size, bool& p_OutRemoveClicked, float p_Rounding = 4.0f, float p_ButtonSize = 20.0f, float p_ButtonPadding = 4.0f)
        {
            if (!p_Image) return false;

            const ImVec2 cursorPos = ImGui::GetCursorScreenPos();
            ImDrawList* drawList   = ImGui::GetWindowDrawList();

            ImGui::InvisibleButton("##Image", p_Size, ImGuiButtonFlags_AllowOverlap);

            const bool hovered = ImGui::IsItemHovered();
            const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

            const float imageWidth = static_cast<float>(p_Image->GetWidth());
            const float imageHeight = static_cast<float>(p_Image->GetHeight());

            ImVec2 imageSize = p_Size;
            if (imageWidth > 0.0f && imageHeight > 0.0f)
            {
                const float scaleX = p_Size.x / imageWidth;
                const float scaleY = p_Size.y / imageHeight;
                const float scale = std::min(scaleX, scaleY);

                imageSize.x = imageWidth * scale;
                imageSize.y = imageHeight * scale;
            }

            const ImVec2 imagePos = {
                cursorPos.x + (p_Size.x - imageSize.x) * 0.5f,
                cursorPos.y + (p_Size.y - imageSize.y) * 0.5f
            };

            const ImVec2 imageEnd = {
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
                p_OutRemoveClicked       = ImGui::IsItemClicked(ImGuiMouseButton_Left);

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
                const ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);

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

        static bool CustomButton(const char* p_Text, const ImVec2& p_Size, float p_Rounding = 4.0f, float p_TextPadding = 8.0f)
        {
            ImGui::PushID(p_Text);

            const ImVec2 cursor   = ImGui::GetCursorScreenPos();
            ImDrawList* drawList  = ImGui::GetWindowDrawList();

            const bool clicked    = ImGui::InvisibleButton("##CustomButton", p_Size);
            const bool hovered    = ImGui::IsItemHovered();
            const bool active     = ImGui::IsItemActive();

            ImU32 backgroundColor =  ImGui::GetColorU32(ImGuiCol_Button);
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

            const float textWidth  = std::max(0.0f, p_Size.x - p_TextPadding * 2.0f);
            const float textHeight = std::max(0.0f, p_Size.y - p_TextPadding * 2.0f);

            const ImVec2 textSize  = ImGui::CalcTextSize(p_Text, nullptr, false, textWidth);
            const ImVec2 textPos   = {
                cursor.x + (p_Size.x - textSize.x) * 0.5f,
                cursor.y + (p_Size.y - textSize.y) * 0.5f
            };

            drawList->AddText(
                ImGui::GetFont(),
                ImGui::GetFontSize(),
                textPos,
                ImGui::GetColorU32(ImGuiCol_Text),
                p_Text,
                nullptr,
                textWidth
            );

            auto borderColor = ImGui::GetStyleColorVec4(ImGuiCol_Border);
            drawList->AddRect(
                cursor,
                cursor + p_Size,
                //ImGui::GetColorU32(ImGuiCol_Border),
                ImGui::GetColorU32({ borderColor.x, borderColor.y, borderColor.z, 0.8f }),
                p_Rounding,
                0,
                active ? 2.0f : hovered ? 1.5f : 1.0f
            );

            ImGui::PopID();

            return clicked;
        }

        static bool ImageButton(const char* p_Text, const std::shared_ptr<Image>& p_Image, const ImVec2& p_Size, bool p_AddText = false, float p_Rounding = 4.0f, float p_TextPadding = 8.0f)
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
                    imageSize.y            = p_Size.y;
                    imageSize.x            = imageSize.y * imageAspect;
                }
                else
                {
                    imageSize.x            = p_Size.x;
                    imageSize.y            = imageSize.x / imageAspect;
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
                const float textWidth = std::max(0.0f, p_Size.x - p_TextPadding * 2.0f);
                const float textHeight = std::max(0.0f, p_Size.y - p_TextPadding * 2.0f);

                const ImVec2 textSize = ImGui::CalcTextSize(p_Text, nullptr, false, textWidth);
                const ImVec2 textPos = {
                    cursor.x + (p_Size.x - textSize.x) * 0.5f,
                    cursor.y + (p_Size.y - textSize.y) * 0.5f
                };

                auto textCol = ImGui::GetStyleColorVec4(ImGuiCol_Text);
                textCol.z    = 0.8f;
                drawList->AddText(
                    ImGui::GetFont(),
                    ImGui::GetFontSize(),
                    textPos,
                    ImGui::GetColorU32(textCol),
                    p_Text,
                    nullptr,
                    textWidth
                );
            }

            auto borderColor = ImGui::GetStyleColorVec4(ImGuiCol_Border);
            borderColor.z    = 0.8f;
            drawList->AddRect(
                cursor,
                cursor + p_Size,
                //ImGui::GetColorU32(ImGuiCol_Border),
                ImGui::GetColorU32(borderColor),
                p_Rounding,
                0,
                active ? 2.0f : hovered ? 1.5f : 1.0f
            );

            ImGui::PopID();

            return clicked;
        }


    } // namespace

    Application::Application()
    {
        Init();
    }

    Application::Application(int p_Argc, char** p_Argv)
    {
        std::string bankPath = "";

        for (int i = 0; i < p_Argc; i++)
        {
            if (strcmp(p_Argv[i], "--help") == 0 || strcmp(p_Argv[i], "-h") == 0)
            {
                std::cout << "Usage: \n";
                std::cout << "  --help[-h]                   | Show this message.\n";
                std::cout << "  --bank[-b] [bank_path]       | Open the QBank with the bank path!\n";
            }

            if (strcmp(p_Argv[i], "--bank") == 0 || strcmp(p_Argv[i], "-b") == 0)
            {
                if (i + 1 < p_Argc)
                    bankPath = std::filesystem::absolute(p_Argv[i + 1]).string();
                else
                    std::cout << "Bank path not provided after --bank or -b flag.\n";
            }
        }

        Init(bankPath.empty());

        if (!bankPath.empty())
        {
            if (std::filesystem::exists(bankPath))
            {
                auto absPath        = std::filesystem::absolute(bankPath).lexically_normal().generic_string();
                ImportBank(absPath);
                ChangeBank(absPath);
            }
            else
                std::cerr << "Failed to open the bank path, the file doesn't exist! " << bankPath << "\n";
        }
    }

    Application::~Application()
    {
        m_Session.StartBank        = m_Bank ? m_Bank->Path : "";
        m_Session.CategorySelected = m_CategorySelected;
        ExportAppSession(m_Session);

        m_ExternalDragDrop.Shutdown();

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();

        ImGui::DestroyContext();

        glfwDestroyWindow(m_Window);
        glfwTerminate();
    }

    void Application::Run()
    {
        while (!glfwWindowShouldClose(m_Window))
        {
            glfwPollEvents();

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            if (m_ExternalDragDrop.BeginSource())
            {
                m_ExternalDragDrop.EndSource();
            }

            // -------------------------
            // UI
            // -------------------------

            BeginDockspace("MyDockspace", "MainDockspace", true, ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_NoResizeX);

            SetupDockspace();

            if (ImGui::BeginMenuBar())
            {
                if (ImGui::BeginMenu("Arquivo"))
                {
                    if (ImGui::MenuItem("Novo"))
                    {
                        m_BankExportWindow = false;
                        m_BankCreateWindow = true;
                    }

                    if (ImGui::BeginMenu("Abrir"))
                    {
                        if (ImGui::MenuItem("Arquivo..."))
                        {
                            std::string path = "";
                            if (FileDialog::Open({ { "Banks", "*.qbank" } }, "Banks", path) == FileDialogResult::SUCCESS)
                            {
                                auto filePath = std::filesystem::absolute(path).lexically_normal().generic_string();
                                ImportBank(filePath);
                                ChangeBank(filePath);
                            }
                        }

                        if (!m_BankNames.empty())
                            ImGui::Separator();

                        for (const auto& path : m_BankNames)
                        {
                            auto stem = std::filesystem::path(path).stem().string();
                            if (ImGui::MenuItem(stem.c_str()))
                            {
                                auto filePath = std::filesystem::absolute(path).lexically_normal().generic_string();
                                ImportBank(filePath);
                                ChangeBank(filePath);
                            }
                        }

                        ImGui::EndMenu();
                    }

                    ImGui::Separator();

                    if (ImGui::MenuItem("Salvar", nullptr, false, m_Bank != nullptr && !m_Bank->Path.empty()))
                    {
                        ExportQuestionBank(*m_Bank);
                    }

                    if (ImGui::MenuItem("Salvar Como"))
                    {
                        m_BankCreateWindow = false;
                        m_BankExportWindow = true;
                    }

                    ImGui::Separator();

                    if (ImGui::MenuItem("Sair"))
                    {
                        glfwSetWindowShouldClose(m_Window, true);
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Editar"))
                {
                    if (ImGui::MenuItem("Adicionar Questão"))
                    {
                        m_QuestionWindow = true;
                    }

                    if (ImGui::MenuItem("Imagens"))
                    {
                        m_ImagesWindow = true;
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::MenuItem("Finalizar Tentativa", nullptr, false, m_Bank != nullptr))
                {
                    m_StatusWindow        = true;
                    m_Bank->CurStatus     = {};
                    m_Bank->CurStatus.TotalAnswers = (int)m_Bank->Questions.size();
                    for (int i = 0; i < m_Bank->CurStatus.TotalAnswers; i++)
                    {
                        auto& question = m_Bank->Questions[i];
                        if (question.OptionMarkedIndex == question.CorrectOptionIndex)
                        {
                            if (question.Options[question.OptionMarkedIndex] == question.CorrectOptionText)
                                m_Bank->CurStatus.CorrectAnswers++;
                        }
                        else
                        {
                            m_Bank->CurStatus.IncorrectQuestions.push_back(i);
                        }
                    }
                }

                ImGui::EndMenuBar();
            }

            SideMenuWindow();

            BanksWindow();

            ImagesWindow();

            QuestionWindow();

            BankExportWindow();

            StatusWindow();

            MainWindow();

            EndDockspace();

            // -------------------------
            // Render
            // -------------------------

            glViewport(0, 0, m_Session.Width, m_Session.Height);

            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            ImGuiIO& io    = ImGui::GetIO();
            io.DisplaySize = ImVec2((float)m_Session.Width, (float)m_Session.Height);

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
            {
                GLFWwindow* backup_current_context = glfwGetCurrentContext();

                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();

                glfwMakeContextCurrent(backup_current_context);
            }

            m_ExternalDragDrop.EndFrame();
            glfwSwapBuffers(m_Window);
        }
    }

    void Application::Init(bool p_LoadStartBank)
    {
        if (!glfwInit())
        {
            std::cerr << "Failed to initialize GLFW\n";
            std::exit(-1);
        }

        m_Session = ImportAppSession();

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_MAXIMIZED, m_Session.Maximize);

        int width = 800, height = 600;
        if (!m_Session.Maximize)
        {
            width = m_Session.Width;
            height = m_Session.Height;
        }
        
        m_Window = glfwCreateWindow(
            width,
            height,
            "QBank",
            nullptr,
            nullptr
        );

        if (!m_Window)
        {
            std::cerr << "Failed to create GLFW window\n";
            glfwTerminate();
            std::exit(-1);
        }

        glfwMakeContextCurrent(m_Window);
        glfwSwapInterval(1);

        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        {
            std::cerr << "Failed to initialize GLAD\n";

            glfwDestroyWindow(m_Window);
            glfwTerminate();
            std::exit(-1);
        }

        glfwSetWindowUserPointer(m_Window, this);

        glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* p_Window, int p_Width, int p_Height)
        {
            Application& data = *(Application*)glfwGetWindowUserPointer(p_Window);
            data.m_Session.Width = p_Width;
            data.m_Session.Height = p_Height;
        });

        glfwSetWindowMaximizeCallback(m_Window, [](GLFWwindow* p_Window, int p_Maximize)
        {
            Application& data = *(Application*)glfwGetWindowUserPointer(p_Window);
            data.m_Session.Maximize = p_Maximize == GLFW_TRUE;
        });

        // FIX IT
        //glfwSetDropCallback(m_Window, [](GLFWwindow* p_Window, int p_PathCount, const char* p_Paths[])
        //{
        //    if (p_PathCount <= 0)
        //        return;

        //    Application& data = *(Application*)glfwGetWindowUserPointer(p_Window);

        //    std::string startPath = "";
        //    for (int i = 0; i < p_PathCount; i++)
        //    {
        //        std::filesystem::path filepath = p_Paths[i];
        //        if (filepath.extension() != ".qbank")
        //            continue;

        //        auto path = filepath.lexically_normal().string();
        //        data.ImportBank(path);

        //        if (i == 0)
        //            startPath = path;
        //    }

        //    if (!startPath.empty())
        //        data.ChangeBank(startPath);
        //});

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.IniFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
        //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
        io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleViewports;
        io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleFonts;
        io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleViewports;

        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForOpenGL(m_Window, true);
        ImGui_ImplOpenGL3_Init("#version 450");

        auto folderPath = std::filesystem::path(FOLDER_PATH);
        if (std::filesystem::exists(folderPath))
        {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(folderPath))
            {
                if (!entry.is_regular_file())
                    continue;

                auto path = entry.path();
                if (path.extension() != ".qbank")
                    continue;

                auto filepath = folderPath / std::filesystem::relative(path, folderPath);
                m_BankNames.push_back(filepath.lexically_normal().generic_string());
            }
        }

        for (const auto& path : m_Session.Banks)
        {
            ImportBank(path, false);
        }

        if (p_LoadStartBank && !m_Session.StartBank.empty() && std::filesystem::exists(m_Session.StartBank))
        {
            ChangeBank(m_Session.StartBank);
        }

        m_Banks["Default"] = {};
        if (!m_Bank)
            m_Bank = &m_Banks["Default"];

        m_ExternalDragDrop.Initialize(m_Window);

        uint32_t whiteTextureData = 0xffffffff;
        m_WhiteImage              = Image::Create({}, (uint8_t*)&whiteTextureData, sizeof(uint32_t));
    }

    void Application::QuestionWindow()
    {
        static bool wasQuestionWindowOpen = false;

        static Question question          = {};
        static char questionText[1024]    = {};
        static char explanationText[1024] = {};
        static char answerText[256]       = {};

        const bool justOpened = m_QuestionWindow && !wasQuestionWindowOpen;
        if (!m_QuestionWindow || !m_Bank)
        {
            wasQuestionWindowOpen = false;
            return;
        }

        if (justOpened)
        {
            question = {};

            std::memset(questionText, 0, sizeof(questionText));
            std::memset(explanationText, 0, sizeof(explanationText));
            std::memset(answerText, 0, sizeof(answerText));

            if (m_QuestionToEdit >= 0 && m_QuestionToEdit < static_cast<int>(m_Bank->Questions.size()))
            {
                question = m_Bank->Questions[m_QuestionToEdit];

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
        ImGui::Begin("##QuestionEditor", &m_QuestionWindow, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);

        ImGui::BeginChild("##Child", ImVec2(0, ImGui::GetContentRegionAvail().y - 23.0f), true);
        const float width = ImGui::GetContentRegionAvail().x;

        const ImVec2 buttonSize = { width, 100.0f };
        ImGui::BeginGroup();
        {
            std::shared_ptr<Image> image = m_WhiteImage;
            if (question.Image > 0)
            {
                auto it = m_Bank->Images.find(question.Image);
                if (it != m_Bank->Images.end() && it->second)
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
                    clipImage = nullptr;

                    auto newImage = GetClipboardImage();
                    if (newImage)
                        clipImage = newImage;

                    recentImages.clear();
                    recentImages.resize(AppSession::MAX_RECENT_IMAGES);

                    for (size_t i = 0; i < AppSession::MAX_RECENT_IMAGES; i++)
                    {
                        auto imageData = m_Session.RecentImages[i];
                        if (imageData.Empty())
                        {
                            recentImages[i] = nullptr;
                            continue;
                        }

                        recentImages[i] = Image::Create(imageData.Data, imageData.Width, imageData.Height);
                    }
                }

                if (CustomButton("Procurar Imagem", { popupWidth / 2.0f, 120.0f }))
                {
                    std::string path;
                    if (FileDialog::Open({ { "Images", "*" } }, "", path) == FileDialogResult::SUCCESS)
                    {
                        const auto hash                 = HashString(path);
                        m_Bank->Images[hash] = LoadImage(path);
                        question.Image                  = hash;
                        ImGui::CloseCurrentPopup();
                    }
                }

                ImGui::SameLine();

                ImGui::BeginDisabled(clipImage == nullptr);
                if (ImageButton("Área de Transferência", clipImage, { popupWidth / 2.0f, 120.0f }, clipImage == nullptr))
                {
                    auto handle = LoadClipboardImage(clipImage);
                    if (handle)
                    {
                        auto it = m_Bank->Images.find(handle);
                        if (it != m_Bank->Images.end())
                            question.Image = handle;
                    }
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndDisabled();

                ImGui::SeparatorText("Imagens Recentes");

                ImGui::BeginChild("##RecentImagesChild", { popupWidth, columnWidth + PADDING }, false);
                auto size = ImGui::GetContentRegionAvail();

                const size_t imageCount = std::min(m_Session.RecentImagesIndex, AppSession::MAX_RECENT_IMAGES);
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

                        bool removeClicked = false;
                        const bool selected = ImageSelectButton(imageTexture, { previewSize, previewSize }, removeClicked);

                        if (selected)
                        {
                            auto curTime = time(nullptr);
                            auto hash = static_cast<uint64_t>(curTime);
                            HashCombine(hash, imageTexture->GetWidth(), imageTexture->GetHeight());

                            if (hash)
                            {
                                m_Bank->Images[hash] = imageTexture;
                                question.Image = hash;
                                PromoteRecentImage(column);
                            }

                            ImGui::PopID();

                            ImGui::CloseCurrentPopup();
                            break;
                        }

                        if (removeClicked)
                        {
                            imageToRemove = static_cast<int>(column);
                        }

                        ImGui::PopID();
                    }

                    ImGui::EndTable();
                }


                if (imageToRemove >= 0)
                {
                    const size_t imageCount = std::min(m_Session.RecentImagesIndex, AppSession::MAX_RECENT_IMAGES);
                    if (imageToRemove < imageCount)
                    {
                        for (size_t i = imageToRemove; i + 1 < imageCount; ++i)
                        {
                            recentImages[i] = recentImages[i + 1];
                        }

                        recentImages[imageCount - 1] = nullptr;
                    }

                    RemoveRecentImage(static_cast<size_t>(imageToRemove));
                }

                ImGui::EndChild();
                
                imageSelectorJustOpened = false;
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
                const auto& files = m_ExternalDragDrop.GetPaths();

                for (const auto& file : files)
                {
                    const std::string path = file.string();
                    const auto hash = HashString(path);

                    m_Bank->Images[hash] = LoadImage(path);
                    question.Image = hash;

                    // One image is enough for this question.
                    break;
                }
            }

            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("IMAGES_ITEM"))
            {
                uint64_t handle = *(uint64_t*)payload->Data;
                if (handle)
                {
                    auto it = m_Bank->Images.find(handle);
                    if (it != m_Bank->Images.end())
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
                auto handle = LoadClipboardImage();
                if (handle)
                {
                    auto it = m_Bank->Images.find(handle);
                    if (it != m_Bank->Images.end())
                        question.Image = handle;
                }
            }
        }

        ImGui::Text("Texto da Questão:");
        ImGui::InputTextMultiline(
            "##Text",
            questionText,
            sizeof(questionText),
            ImVec2(width, 50.0f),
            ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_WordWrap
        );

        ImGui::Text("Explicação:");
        ImGui::InputTextMultiline(
            "##Explanation",
            explanationText,
            sizeof(explanationText),
            ImVec2(width, 50.0f),
            ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_WordWrap
        );

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
            static char newCategory[128]  = {};

            ImGui::Text("Search:");
            ImGui::InputText(
                "##Search",
                searchBuffer,
                sizeof(searchBuffer),
                ImGuiInputTextFlags_AutoSelectAll
            );

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
                std::stringstream ss(m_Bank->Categories);
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

            ImGui::BeginChild("##AddCategory", {0, ImGui::GetFrameHeight() + 4 * ImGui::GetStyle().ItemSpacing.y }, true);

            auto createCategory = [&]()
            {
                std::string category(newCategory);

                if (category.empty())
                    return;

                // Add to bank only if it does not already exist.
                bool exists = false;

                {
                    std::stringstream ss(m_Bank->Categories);
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
                    if (!m_Bank->Categories.empty())
                        m_Bank->Categories += ';';

                    m_Bank->Categories += category;
                }

                addCategory(category);

                newCategory[0] = '\0';
            };

            if (ImGui::InputText(
                "##Category",
                newCategory,
                sizeof(newCategory),
                ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
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

        if (ImGui::InputText(
            "##NewAnswer",
            answerText,
            sizeof(answerText),
            ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
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

                    bool marked = question.CorrectOptionIndex == optionIndex;
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
                    newCorrectIndex        = -1;
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
                bool correctWasDeleted     = std::binary_search(questionsToDelete.begin(), questionsToDelete.end(), oldCorrectIndex);
                if (!correctWasDeleted)
                {
                    int shift              = 0;
                    for (int deletedIndex : questionsToDelete)
                    {
                        if (deletedIndex < oldCorrectIndex)
                            shift++;
                    }

                    newCorrectIndex        = oldCorrectIndex - shift;
                }
                else
                {
                    newCorrectIndex        = -1;
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
            m_QuestionWindow   = false;
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
            question.Text        = questionText;
            question.Explanation = explanationText;

            // Keep CorrectOptionText synchronized.
            if (question.CorrectOptionIndex >= 0 && question.CorrectOptionIndex < question.CurrentOptionIndex)
            {
                question.CorrectOptionText = question.Options[question.CorrectOptionIndex];
            }
            else
            {
                question.CorrectOptionText.clear();
                question.CorrectOptionIndex = -1;
            }

            if (m_QuestionToEdit >= 0 && m_QuestionToEdit < static_cast<int>(m_Bank->Questions.size()))
            {
                m_Bank->Questions[m_QuestionToEdit] = question;
            }
            else
            {
                m_Bank->Questions.push_back(question);
            }


            question           = {};

            questionText[0]    = '\0';
            explanationText[0] = '\0';
            answerText[0]      = '\0';

            m_QuestionToEdit   = -1;
            m_QuestionWindow   = false;
        }

        ImGui::EndDisabled();

        ImGui::End();

        wasQuestionWindowOpen = m_QuestionWindow;
    }

    void Application::StatusWindow()
    {
        if (!m_StatusWindow) return;
        if (!m_Bank) return;

        auto& status = m_Bank->CurStatus;

        ImGui::SetNextWindowSizeConstraints({ 350, 450 }, { FLT_MAX, FLT_MAX });
        ImGui::Begin("##StatusWindow", &m_StatusWindow, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);

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
                    auto& question = m_Bank->Questions[i];
                    ImGui::PushID((int)i);

                    if (ImGui::TreeNodeEx(("Questão " + std::to_string(i + 1)).c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_Bullet))
                    {
                        float questionDetailsHeight = CalculateQuestionDetailsHeight(question, ImGui::GetContentRegionAvail().x);
                        questionDetailsHeight       = question.Image != 0 ? questionDetailsHeight + QUESTION_IMAGE_HEIGHT : questionDetailsHeight;

                        ImGui::BeginChild("##QuestionDetails", ImVec2(0, questionDetailsHeight), true);
                        {
                            DrawCategoryButtons(question.Categories, ImGui::GetContentRegionAvail().x);

                            if (question.Image != 0 && m_Bank->Images[question.Image])
                            {
                                float width           = ImGui::GetContentRegionAvail().x;

                                ImGui::BeginGroup();
                                ImGui::PushID((int)question.Image);

                                ImVec2 viewport       = { width, QUESTION_IMAGE_HEIGHT };
                                auto& image           = m_Bank->Images[question.Image];

                                ImVec2 cursor         = ImGui::GetCursorScreenPos();
                                ImDrawList* drawList  = ImGui::GetWindowDrawList();

                                ImU32 backgroundColor = IM_COL32(30, 30, 30, 255);

                                drawList->AddRectFilled(
                                    cursor,
                                    cursor + viewport,
                                    backgroundColor,
                                    4.0f);

                                float imageWidth     = static_cast<float>(image->GetWidth());
                                float imageHeight    = static_cast<float>(image->GetHeight());

                                float targetAspect   = imageWidth / imageHeight;
                                float viewportAspect = viewport.x / viewport.y;

                                ImVec2 imageSize;
                                if (viewportAspect > targetAspect)
                                {
                                    imageSize.y      = viewport.y;
                                    imageSize.x      = imageSize.y * targetAspect;
                                }
                                else
                                {
                                    imageSize.x      = viewport.x;
                                    imageSize.y      = imageSize.x / targetAspect;
                                }

                                ImVec2 imagePos;
                                imagePos.x           = cursor.x + (viewport.x - imageSize.x) * 0.5f;
                                imagePos.y           = cursor.y + (viewport.y - imageSize.y) * 0.5f;

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
            m_Bank->CurStatus = {};
            for (auto& question : m_Bank->Questions)
            {
                question.OptionMarkedIndex = -1;
            }
            m_StatusWindow = false;
        }
        
        ImGui::End();
    }

    void Application::BankExportWindow()
    {
        if (!m_Bank) return;
        if (!m_BankExportWindow && !m_BankCreateWindow) return;

        static char buffer[256] = {};

        ImGui::SetNextWindowSizeConstraints({ 300, 70 }, { FLT_MAX, FLT_MAX });
        if (m_BankExportWindow)
            ImGui::Begin("##BankExport", &m_BankExportWindow, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoDecoration );
        if (m_BankCreateWindow)
            ImGui::Begin("##BankCreate", &m_BankCreateWindow, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoDecoration);

        ImGui::Text("Name:");
        ImGui::InputText("##Name", buffer, sizeof(buffer), ImGuiInputTextFlags_AutoSelectAll);

        if (ImGui::Button("Cancelar"))
        {
            m_BankExportWindow = false;
            m_BankCreateWindow = false;
            buffer[0]          = '\0';
        }
        ImGui::SameLine();

        if (m_BankCreateWindow)
        {
            bool alreadyExist = false;
            if (buffer[0] != '\0')
            {
                std::string name(buffer);
                std::filesystem::path folder = FOLDER_PATH;
                auto absPath = std::filesystem::absolute(folder / (name + ".qbank")).lexically_normal().generic_string();
            
                alreadyExist = m_Banks.find(absPath) != m_Banks.end();
            }

            ImGui::BeginDisabled(buffer[0] == '\0' || alreadyExist);
            if (ImGui::Button("Criar"))
            {
                std::string name(buffer);
                std::filesystem::path folder = FOLDER_PATH;
                auto path = std::filesystem::absolute(folder / (name + ".qbank")).lexically_normal().generic_string();
                m_Banks[path] = {};
                m_Banks[path].Path = path;
                m_Session.Banks.push_back(path);

                ExportQuestionBank(m_Banks[path]);
                ChangeBank(path);

                m_BankCreateWindow = false;
                buffer[0]          = '\0';
            }
            ImGui::EndDisabled();
        }
        else
        {
            ImGui::BeginDisabled(!m_Bank || buffer[0] == '\0');
            if (ImGui::Button("Salvar"))
            {
                std::string name(buffer);
                std::filesystem::path folder = FOLDER_PATH;
                m_Bank->Path = std::filesystem::absolute(folder / (name + ".qbank")).lexically_normal().generic_string();
                ExportQuestionBank(*m_Bank);

                m_BankExportWindow      = false;
                buffer[0]               = '\0';
            }
            ImGui::EndDisabled();
        }

        ImGui::End();
    }

    void Application::MainWindow()
    {
        ImGui::Begin("QBank");

        if (m_Bank)
        {
            float categoriesHeight = CalculateCategoryButtonsHeight(m_Bank->Categories, ImGui::GetContentRegionAvail().x);
            ImGui::BeginChild("Categories", ImVec2(0, categoriesHeight));
            DrawCategoryButtons(m_Bank->Categories, ImGui::GetContentRegionAvail().x);
            ImGui::EndChild();

            if (m_FilteredQuestions.empty() && !m_CategorySelected.empty())
            {
                ImGui::Text("Não foi encontrado nenhum resultado para: '%s'", m_CategorySelected.c_str());
            }
            else
            {
                size_t size = m_FilteredQuestions.empty() ? m_Bank->Questions.size() : m_FilteredQuestions.size();
                for (size_t i = 0; i < size; ++i)
                {
                    auto& question = m_Bank->Questions[m_FilteredQuestions.empty() ? i : m_FilteredQuestions[i]];
                    ImGui::PushID((int)i);

                    bool open = ImGui::TreeNodeEx(("Questão " + std::to_string(i + 1)).c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_Bullet);
                    ImGui::SameLine();
                    if (ImGui::Button("Editar"))
                    {
                        m_QuestionToEdit = m_FilteredQuestions.empty() ? (int)i : m_FilteredQuestions[i];
                        m_QuestionWindow = true;
                    }

                    if (open)
                    {
                        float questionDetailsHeight = CalculateQuestionDetailsHeight(question, ImGui::GetContentRegionAvail().x);
                        questionDetailsHeight = question.Image != 0 ? questionDetailsHeight + QUESTION_IMAGE_HEIGHT : questionDetailsHeight;

                        ImGui::BeginChild("##QuestionDetails", ImVec2(0, questionDetailsHeight), true);
                        {
                            DrawCategoryButtons(question.Categories, ImGui::GetContentRegionAvail().x);

                            if (question.Image != 0 && m_Bank->Images[question.Image])
                            {
                                float width = ImGui::GetContentRegionAvail().x;

                                ImGui::BeginGroup();
                                ImGui::PushID((int)question.Image);

                                ImVec2 viewport = { width, QUESTION_IMAGE_HEIGHT };
                                auto& image = m_Bank->Images[question.Image];

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

                                        bool marked = question.OptionMarkedIndex == index;
                                        if (ImGui::Checkbox("", &marked))
                                        {
                                            question.OptionMarkedIndex = question.OptionMarkedIndex == index ? -1 : index;
                                        }
                                        ImGui::SameLine();
                                        ImGui::Text("(%c)", 'A' + static_cast<char>(optionIndex));
                                        ImGui::SameLine();
                                        ImGui::PushTextWrapPos();
                                        ImGui::TextUnformatted(question.Options[optionIndex].c_str());
                                        ImGui::PopTextWrapPos();

                                        ImGui::PopID();
                                    }
                                }
                                ImGui::EndTable();
                            }
                        }
                        ImGui::EndChild();

                        ImGui::TreePop();
                    }

                    ImGui::PopID();
                }
            }
        }
        
        ImGui::End();
    }

    void Application::SideMenuWindow()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0.0f, 0.0f });
        ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, { 0.0f, 0.0f });
        ImGui::Begin("SideMenu");

        const char* text         = "Bancos";
        const float buttonWidth  = SIDEBAR_WIDTH;
        const float buttonHeight = ImGui::CalcTextSize(text).x + 20.0f;

        if (VerticalButton(text, ImVec2(buttonWidth, buttonHeight), m_ShowBanks))
        {
            m_ShowBanks = !m_ShowBanks;
        }

        ImGui::End();
        ImGui::PopStyleVar(3);
    }

    void Application::SetupDockspace()
    {
        ImGuiID dockspaceID = ImGui::GetID("MyDockspace");

        static bool first = true;
        if (first)
        {
            first = false;

            ImGui::DockBuilderRemoveNode(dockspaceID);

            ImGuiViewport* viewport = ImGui::GetMainViewport();

            ImGui::DockBuilderAddNode(
                dockspaceID,
                ImGuiDockNodeFlags_DockSpace
            );

            ImGui::DockBuilderSetNodeSize(
                dockspaceID,
                viewport->WorkSize
            );

            ImGuiID leftID;
            ImGuiID centerID;

            const float sidebarWidth = SIDEBAR_WIDTH;
            const float ratio = sidebarWidth / viewport->WorkSize.x;

            ImGui::DockBuilderSplitNode(
                dockspaceID,
                ImGuiDir_Left,
                ratio,
                &leftID,
                &centerID
            );

            ImGui::DockBuilderDockWindow("SideMenu", leftID);
            ImGui::DockBuilderDockWindow("QBank", centerID);

            ImGui::DockBuilderFinish(dockspaceID);
        }
    }

    void Application::BanksWindow()
    {
        if (!m_ShowBanks)
            return;

        int windowX, windowY;
        glfwGetWindowPos(m_Window, &windowX, &windowY);

        const float frameHeight = ImGui::GetFrameHeight();
        const float x = static_cast<float>(windowX) + SIDEBAR_WIDTH;
        const float y = static_cast<float>(windowY) + frameHeight;

        ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);

        const float width = static_cast<float>(m_Session.Width) / 4.0f;
        const float height = static_cast<float>(m_Session.Height) - frameHeight;

        ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);

        auto color = ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg);
        ImGui::PushStyleColor(ImGuiCol_TitleBg, color);
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, color);
        ImGui::Begin("Bancos", &m_ShowBanks, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
        ImGui::PopStyleColor(2);

        auto size = ImGui::GetContentRegionAvail().x;
        for (const auto& path : m_Session.Banks)
        {
            ImGui::PushID(path.c_str());

            auto name = std::filesystem::path(path).stem().string();
            if (ImGui::Button(name.c_str(), { size, 0.0f }))
            {
                //ImportBank(path);
                ChangeBank(path);
            }

            ImGui::PopID();
        }

        ImGui::End();
    }

    void Application::ImagesWindow()
    {
        if (!m_ImagesWindow) return;
        if (!m_Bank) return;

        ImGui::SetNextWindowSizeConstraints({ 350.0f, 450.0f }, { FLT_MAX, FLT_MAX });
        ImGui::Begin("##ImagesWindow", &m_ImagesWindow, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse);

        constexpr ImVec2 viewportSize = { 100.0f, 100.0f };
        const float panelWidth        = ImGui::GetContentRegionAvail().x;
        const ImVec2 cellSize         = viewportSize + ImVec2(PADDING, PADDING);

        int columnCount               = static_cast<int>(panelWidth / cellSize.x);
        columnCount                   = std::max(1, columnCount);

        std::vector<uint64_t> images;
        images.reserve(m_Bank->Images.size());

        for (const auto& [handle, image] : m_Bank->Images)
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
                        auto imageIt          = m_Bank->Images.find(handle);
                        if (imageIt == m_Bank->Images.end())
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
                m_Bank->Images[handle] = nullptr;
                m_Bank->Images.erase(handle);
            }
        }

        ImGui::End();
    }

    void Application::ImportBank(const std::string& p_AbsPath, bool p_SaveToSession)
    {
        auto path = std::filesystem::absolute(p_AbsPath).lexically_normal().generic_string();
        m_Banks[path] = ImportQuestionBank(path);
        if (p_SaveToSession)
        {
            auto it = std::find_if(m_Session.Banks.begin(), m_Session.Banks.end(), [&](const auto& p_Path)
            {
                return p_Path == path;
            });

            if (it == m_Session.Banks.end())
                m_Session.Banks.push_back(path);
        }
    }

    void Application::RemoveBank(const std::string& p_AbsPath)
    {
        if (m_Session.StartBank == p_AbsPath)
        {
            m_Bank              = nullptr;
            m_Session.StartBank = "";
        }

        m_Banks.erase(p_AbsPath);
        std::erase_if(m_Session.Banks, [&](const std::string& p_Bank)
        {
            return p_Bank == p_AbsPath;
        });
    }

    void Application::ChangeBank(const std::string& p_AbsPath)
    {
        auto path               = std::filesystem::absolute(p_AbsPath).lexically_normal().generic_string();
        auto it                 = m_Banks.find(path);
        if (it != m_Banks.end())
        {
            m_Bank              = &m_Banks[path];
            m_Session.StartBank = path;
        }
    }

    std::shared_ptr<Image> Application::GetClipboardImage()
    {
        auto data  = Clipboard::GetImage();
        if (!data)
            return nullptr;

        auto image = Image::Create(data->Pixels, data->Width, data->Height);
        if (!image)
            return nullptr;

        return image;
    }

    uint64_t Application::LoadClipboardImage()
    {
        if (!m_Bank)
            return 0;

        auto image = GetClipboardImage();
        if (!image)
            return 0;

        return LoadClipboardImage(image);
    }

    uint64_t Application::LoadClipboardImage(const std::shared_ptr<Image>& p_Image)
    {
        if (!m_Bank)
            return 0;

        if (!p_Image)
            return 0;

        ImageData data = {};
        data.Width     = p_Image->GetWidth();
        data.Height    = p_Image->GetHeight();
        data.Data      = p_Image->GetData();

        auto curTime   = time(nullptr);

        auto hash      = static_cast<uint64_t>(curTime);
        HashCombine(hash, data.Width, data.Height);


        const size_t imageCount = std::min(m_Session.RecentImagesIndex, AppSession::MAX_RECENT_IMAGES);
        const size_t newCount   = std::min(imageCount + 1, AppSession::MAX_RECENT_IMAGES);

        for (size_t i = newCount; i > 0; --i)
        {
            const size_t destination = i - 1;
            if (destination == 0)
                continue;

            m_Session.RecentImages[destination] = m_Session.RecentImages[destination - 1];
        }

        m_Session.RecentImages[0]   = data;
        m_Session.RecentImagesIndex = newCount;
        m_Bank->Images[hash]        = p_Image;

        return hash;
    }

    void Application::RemoveRecentImage(size_t p_Index)
    {
        const size_t imageCount                 = std::min(m_Session.RecentImagesIndex, AppSession::MAX_RECENT_IMAGES);
        if (p_Index >= imageCount)
            return;

        for (size_t i = p_Index; i + 1 < imageCount; ++i)
        {
            m_Session.RecentImages[i]          = m_Session.RecentImages[i + 1];
        }

        m_Session.RecentImages[imageCount - 1] = {};
        m_Session.RecentImagesIndex            = imageCount - 1;
    }

    void Application::PromoteRecentImage(size_t p_Index)
    {
        const size_t imageCount = std::min(m_Session.RecentImagesIndex, AppSession::MAX_RECENT_IMAGES);
        if (p_Index >= imageCount)
            return;

        if (p_Index == 0)
            return;

        ImageData selected = m_Session.RecentImages[p_Index];
        for (size_t i = p_Index; i > 0; --i)
        {
            m_Session.RecentImages[i] = m_Session.RecentImages[i - 1];
        }

        m_Session.RecentImages[0] = selected;
    }

    void Application::DrawCategoryButtons(const std::string& p_Categories, float p_Width)
    {
        std::stringstream ss(p_Categories);
        const ImGuiStyle& style = ImGui::GetStyle();
        const float spacing     = style.ItemSpacing.x; std::string category;
        float lineWidth         = 0.0f;
        bool first              = true;

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

        while (std::getline(ss, category, ';'))
        {
            const ImVec2 textSize = ImGui::CalcTextSize(category.c_str());
            const float buttonWidth = textSize.x + style.FramePadding.x * 2.0f;
            const float requiredWidth = first ? buttonWidth : spacing + buttonWidth;
            if (!first && lineWidth + requiredWidth > p_Width)
                lineWidth = buttonWidth;
            else
            {
                if (!first)
                    ImGui::SameLine();
                lineWidth += requiredWidth;
            }

            bool selected = m_CategorySelected == category;

            auto activeColor = IM_COL32(60, 60, 60, 255);
            ImGui::PushStyleColor(ImGuiCol_Button, selected ? activeColor : IM_COL32(30, 30, 30, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(45, 45, 45, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeColor);

            if (ImGui::Button(category.c_str(), ImVec2(buttonWidth, 0.0f)))
            {
                m_FilteredQuestions.clear();
                if (!selected && m_Bank)
                {
                    for (size_t i = 0; i < m_Bank->Questions.size(); i++)
                    {
                        auto& question = m_Bank->Questions[i];
                        if (hasCategory(question.Categories, category))
                        {
                            m_FilteredQuestions.push_back((int)i);
                        }
                    }
                }
                m_CategorySelected = selected ? "" : category;
            }

            ImGui::PopStyleColor(3);
            first = false;
        }
    }

} // namespace QB
