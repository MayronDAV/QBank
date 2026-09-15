#include "Application.h"
#include "FileDialog.h"

// std
#include <iostream>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <filesystem>

// lib
#include <glad/glad.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <stb/stb_image.h>
#include <stb/stb_image_resize2.h>



namespace QB
{
    namespace
    {
        static constexpr float PADDING               = 10.0f;
        static constexpr const char* FOLDER_PATH     = "Banks";
        static uint32_t s_MaxImageWidth              = 2048;
        static uint32_t s_MaxImageHeight             = 2048;
        static constexpr float QUESTION_IMAGE_HEIGHT = 100.0f;

        static void  BeginDockspace(std::string p_ID, std::string p_Dockspace, bool p_MenuBar, ImGuiDockNodeFlags p_DockFlags)
        {
            static bool opt_fullscreen = true;
            static bool opt_padding = false;
            static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_NoWindowMenuButton | ImGuiDockNodeFlags_NoCloseButton;

            ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
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
            std::filesystem::path folder = FOLDER_PATH;
            if (!std::filesystem::exists(folder))
                std::filesystem::create_directories(folder);

            const auto path = folder / (p_Bank.Name + ".qbank");
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            if (!out)
            {
                std::cerr << "Failed to export bank at " << path.string() << "\n";
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
            bank.Name = std::filesystem::path(p_Path).stem().string();

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
            uint32_t Version      = 1;
        };

        static bool  ExportAppConfig(const ApplicationConfig& p_Config)
        {
            const std::string path = "ini.qbapp";
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            if (!out)
            {
                std::cerr << "Failed to export app config at " << path << "\n";
                return false;
            }

            APPHeader header;
            out.write(reinterpret_cast<const char*>(&header), sizeof(header));

            WriteString(out, p_Config.StartBank);
            out.write(reinterpret_cast<const char*>(&p_Config.Maximize), sizeof(p_Config.Maximize));
            out.write(reinterpret_cast<const char*>(&p_Config.Width), sizeof(p_Config.Width));
            out.write(reinterpret_cast<const char*>(&p_Config.Height), sizeof(p_Config.Height));

            return true;
        }

        static ApplicationConfig ImportAppConfig()
        {
            const std::string path   = "ini.qbapp";
            ApplicationConfig config = {};

            if (!std::filesystem::exists(path))
                return config;

            std::ifstream in(path, std::ios::binary);
            if (!in)
            {
                std::cerr << "Failed to import app config: " << path << "\n";
                return config;
            }

            APPHeader header;
            in.read(reinterpret_cast<char*>(&header), sizeof(header));

            config.StartBank = ReadString(in);
            in.read(reinterpret_cast<char*>(&config.Maximize), sizeof(config.Maximize));
            in.read(reinterpret_cast<char*>(&config.Width), sizeof(config.Width));
            in.read(reinterpret_cast<char*>(&config.Height), sizeof(config.Height));

            return config;
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
            spec.Format             = (isHDR) ? TextureFormat::RGBA32_FLOAT : TextureFormat::RGBA8;

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
                m_Data.CurrentBank = ImportQuestionBank(bankPath);
            else
                std::cerr << "Failed to open the bank path, the file doesn't exist! " << bankPath << "\n";
        }
    }

    Application::~Application()
    {
        std::filesystem::path folder = FOLDER_PATH;
        m_Data.Config.StartBank      = m_Data.CurrentBank.Name.empty() ? "" : (folder / (m_Data.CurrentBank.Name + ".qbank")).lexically_normal().generic_string();
        ExportAppConfig(m_Data.Config);

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

            BeginDockspace("MyDockspace", "MainDockspace", true, ImGuiDockNodeFlags_NoTabBar);

            if (ImGui::BeginMenuBar())
            {
                if (ImGui::BeginMenu("Arquivo"))
                {
                    if (ImGui::MenuItem("Novo"))
                    {
                        m_Data.CurrentBank = {};
                    }

                    if (ImGui::BeginMenu("Abrir"))
                    {
                        if (ImGui::MenuItem("Arquivo..."))
                        {
                            std::string path = "";
                            if (FileDialog::Open({ { "Banks", "*.qbank" } }, "Banks", path) == FileDialogResult::SUCCESS)
                            {
                                m_Data.CurrentBank = ImportQuestionBank(path);
                            }
                        }

                        if (!m_BankNames.empty())
                            ImGui::Separator();

                        for (const auto& path : m_BankNames)
                        {
                            auto stem = std::filesystem::path(path).stem().string();
                            if (ImGui::MenuItem(stem.c_str()))
                            {
                                m_Data.CurrentBank = ImportQuestionBank(path);
                            }
                        }

                        ImGui::EndMenu();
                    }

                    ImGui::Separator();

                    ImGui::BeginDisabled(m_Data.CurrentBank.Name.empty());
                    if (ImGui::MenuItem("Salvar"))
                    {
                        ExportQuestionBank(m_Data.CurrentBank);
                    }
                    ImGui::EndDisabled();

                    if (ImGui::MenuItem("Salvar Como"))
                    {
                        m_BankExportWindow = true;
                    }

                    ImGui::Separator();

                    if (ImGui::MenuItem("Sair"))
                    {
                        glfwSetWindowShouldClose(m_Window, true);
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::MenuItem("Adicionar Questão"))
                {
                    m_QuestionWindow = true;
                }

                if (ImGui::MenuItem("Finalizar Tentativa"))
                {
                    m_StatusWindow        = true;
                    m_Status              = {};
                    m_Status.TotalAnswers = (int)m_Data.CurrentBank.Questions.size();
                    for (int i = 0; i < m_Status.TotalAnswers; i++)
                    {
                        auto& question = m_Data.CurrentBank.Questions[i];
                        if (question.OptionMarkedIndex == question.CorrectOptionIndex)
                        {
                            if (question.Options[question.OptionMarkedIndex] == question.CorrectOptionText)
                                m_Status.CorrectAnswers++;
                        }
                        else
                        {
                            m_Status.IncorrectQuestions.push_back(i);
                        }
                    }
                }

                ImGui::EndMenuBar();
            }

            QuestionWindow();

            BankExportWindow();

            StatusWindow();

            MainWindow();

            EndDockspace();

            // -------------------------
            // Render
            // -------------------------

            glViewport(0, 0, m_Data.Config.Width, m_Data.Config.Height);

            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            ImGuiIO& io    = ImGui::GetIO();
            io.DisplaySize = ImVec2((float)m_Data.Config.Width, (float)m_Data.Config.Height);

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

        m_Data.Config = ImportAppConfig();

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_MAXIMIZED, m_Data.Config.Maximize);

        m_Window = glfwCreateWindow(
            m_Data.Config.Width,
            m_Data.Config.Height,
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

        glfwSetWindowUserPointer(m_Window, &m_Data);

        glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* p_Window, int p_Width, int p_Height)
        {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(p_Window);
            data.Config.Width = p_Width;
            data.Config.Height = p_Height;
        });

        glfwSetWindowMaximizeCallback(m_Window, [](GLFWwindow* p_Window, int p_Maximize)
        {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(p_Window);
            data.Config.Maximize = p_Maximize == GLFW_TRUE;
        });

        glfwSetDropCallback(m_Window, [](GLFWwindow* p_Window, int p_PathCount, const char* p_Paths[])
        {
            if (p_PathCount <= 0)
                return;

            std::filesystem::path filepath = p_Paths[0];
            if (filepath.extension() != ".qbank")
                return;

            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(p_Window);
            data.CurrentBank = ImportQuestionBank(filepath.string());
        });

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

        if (p_LoadStartBank && !m_Data.Config.StartBank.empty() && std::filesystem::exists(m_Data.Config.StartBank))
            m_Data.CurrentBank = ImportQuestionBank(m_Data.Config.StartBank);

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
        if (!m_QuestionWindow)
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

            if (m_QuestionToEdit >= 0 && m_QuestionToEdit < static_cast<int>(m_Data.CurrentBank.Questions.size()))
            {
                question = m_Data.CurrentBank.Questions[m_QuestionToEdit];

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
                auto it = m_Data.CurrentBank.Images.find(question.Image);
                if (it != m_Data.CurrentBank.Images.end() && it->second)
                    image = it->second;
            }

            ImGui::PushID("QuestionImage");

            const ImVec2 cursor = ImGui::GetCursorScreenPos();
            const bool clicked  = ImGui::InvisibleButton("##ImageButton", buttonSize);
            const bool hovered  = ImGui::IsItemHovered();
            const bool active   = ImGui::IsItemActive();

            if (clicked)
            {
                std::string path;

                if (FileDialog::Open({ { "Images", "*" } }, "", path) == FileDialogResult::SUCCESS)
                {
                    const auto hash                 = HashString(path);
                    m_Data.CurrentBank.Images[hash] = LoadImage(path);
                    question.Image                  = hash;
                }
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

                    m_Data.CurrentBank.Images[hash] = LoadImage(path);
                    question.Image = hash;

                    // One image is enough for this question.
                    break;
                }
            }

            ImGui::EndDragDropTarget();
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
                std::stringstream ss(m_Data.CurrentBank.Categories);
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
                    std::stringstream ss(m_Data.CurrentBank.Categories);
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
                    if (!m_Data.CurrentBank.Categories.empty())
                        m_Data.CurrentBank.Categories += ';';

                    m_Data.CurrentBank.Categories += category;
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

            if (m_QuestionToEdit >= 0 && m_QuestionToEdit < static_cast<int>(m_Data.CurrentBank.Questions.size()))
            {
                m_Data.CurrentBank.Questions[m_QuestionToEdit] = question;
            }
            else
            {
                m_Data.CurrentBank.Questions.push_back(question);
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

        ImGui::SetNextWindowSizeConstraints({ 350, 450 }, { FLT_MAX, FLT_MAX });
        ImGui::Begin("##StatusWindow", &m_StatusWindow, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);

        const auto size = ImGui::GetContentRegionAvail();
        ImGui::BeginChild("##StatusInfo", { 0.0f, size.y - 23.0f }, false);

        ImGui::Text("Respostas Corretas: %i/%i", m_Status.CorrectAnswers, m_Status.TotalAnswers);
        
        if (m_Status.CorrectAnswers == m_Status.TotalAnswers)
        {
            ImGui::TextColored({ 0.0f, 1.0f, 0.0f, 1.0f }, "Parabéns, você acertou todas as questões!!!");
        }
        else
        {
            if (ImGui::TreeNodeEx("Respostas Incorretas", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_FramePadding))
            {
                ImGui::BeginChild("##IncorrectAnswers", { 0.0f, 0.0f }, true);

                for (size_t i = 0; i < m_Status.IncorrectQuestions.size(); ++i)
                {
                    auto& question = m_Data.CurrentBank.Questions[i];
                    ImGui::PushID((int)i);

                    if (ImGui::TreeNodeEx(("Questão " + std::to_string(i + 1)).c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_Bullet))
                    {
                        float questionDetailsHeight = CalculateQuestionDetailsHeight(question, ImGui::GetContentRegionAvail().x);
                        questionDetailsHeight       = question.Image != 0 ? questionDetailsHeight + QUESTION_IMAGE_HEIGHT : questionDetailsHeight;

                        ImGui::BeginChild("##QuestionDetails", ImVec2(0, questionDetailsHeight), true);
                        {
                            DrawCategoryButtons(question.Categories, ImGui::GetContentRegionAvail().x);

                            if (question.Image != 0 && m_Data.CurrentBank.Images[question.Image])
                            {
                                float width           = ImGui::GetContentRegionAvail().x;

                                ImGui::BeginGroup();
                                ImGui::PushID((int)question.Image);

                                ImVec2 viewport       = { width, QUESTION_IMAGE_HEIGHT };
                                auto& image           = m_Data.CurrentBank.Images[question.Image];

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
            m_Status = {};
            for (auto& question : m_Data.CurrentBank.Questions)
            {
                question.OptionMarkedIndex = -1;
            }
            m_StatusWindow = false;
        }
        ImGui::End();
    }

    void Application::BankExportWindow()
    {
        if (!m_BankExportWindow) return;

        static char buffer[256] = {};

        ImGui::SetNextWindowSizeConstraints({ 300, 70 }, { FLT_MAX, FLT_MAX });
        ImGui::Begin("##BankExport", &m_BankExportWindow, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoDecoration );

        ImGui::Text("Name:");
        ImGui::InputText("##Name", buffer, sizeof(buffer), ImGuiInputTextFlags_AutoSelectAll);

        if (ImGui::Button("Cancelar"))
        {
            m_BankExportWindow = false;
            buffer[0]          = '\0';
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(buffer[0] == '\0');
        if (ImGui::Button("Salvar"))
        {
            std::string name(buffer);
            m_Data.CurrentBank.Name = name;
            ExportQuestionBank(m_Data.CurrentBank);
            m_BankExportWindow      = false;
            buffer[0]               = '\0';
        }
        ImGui::EndDisabled();

        ImGui::End();
    }

    void Application::MainWindow()
    {
        ImGui::SetNextWindowDockID(ImGui::GetID("MyDockspace"), ImGuiCond_Once);
        ImGui::Begin("QBank");

        float categoriesHeight = CalculateCategoryButtonsHeight(m_Data.CurrentBank.Categories, ImGui::GetContentRegionAvail().x);
        ImGui::BeginChild("Categories", ImVec2(0, categoriesHeight));
        DrawCategoryButtons(m_Data.CurrentBank.Categories, ImGui::GetContentRegionAvail().x);
        ImGui::EndChild();

        if (m_FilteredQuestions.empty() && !m_CategorySelected.empty())
        {
            ImGui::Text("Não foi encontrado nenhum resultado para: '%s'", m_CategorySelected.c_str());
        }
        else
        {
            size_t size = m_FilteredQuestions.empty() ? m_Data.CurrentBank.Questions.size() : m_FilteredQuestions.size();
            for (size_t i = 0; i < size; ++i)
            {
                auto& question = m_Data.CurrentBank.Questions[m_FilteredQuestions.empty() ? i : m_FilteredQuestions[i]];
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

                        if (question.Image != 0 && m_Data.CurrentBank.Images[question.Image])
                        {
                            float width = ImGui::GetContentRegionAvail().x;

                            ImGui::BeginGroup();
                            ImGui::PushID((int)question.Image);

                            ImVec2 viewport = { width, QUESTION_IMAGE_HEIGHT };
                            auto& image = m_Data.CurrentBank.Images[question.Image];

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

        ImGui::End();
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
                if (!selected)
                {
                    for (size_t i = 0; i < m_Data.CurrentBank.Questions.size(); i++)
                    {
                        auto& question = m_Data.CurrentBank.Questions[i];
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
