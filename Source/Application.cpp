#include "Application.h"

// std
#include <iostream>
#include <sstream>
#include <algorithm>

// lib
#include <glad/glad.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>



namespace QB
{
    namespace
    {
        static constexpr float PADDING = 10.0f;

        void  BeginDockspace(std::string p_ID, std::string p_Dockspace, bool p_MenuBar, ImGuiDockNodeFlags p_DockFlags)
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

        void  EndDockspace()
        {
            ImGui::End();
        }

        void  DrawCategoryButtons(const std::string& categories, float p_Width)
        {
            std::stringstream ss(categories);
            const ImGuiStyle& style       = ImGui::GetStyle();
            const float spacing           = style.ItemSpacing.x; std::string category;
            float lineWidth               = 0.0f;
            bool first                    = true;
            
            while (std::getline(ss, category, ';'))
            {
                const ImVec2 textSize     = ImGui::CalcTextSize(category.c_str());
                const float buttonWidth   = textSize.x + style.FramePadding.x * 2.0f;
                const float requiredWidth = first ? buttonWidth : spacing + buttonWidth;
                if (!first && lineWidth + requiredWidth > p_Width)
                    lineWidth             = buttonWidth;
                else
                {
                    if (!first)
                        ImGui::SameLine();
                    lineWidth            += requiredWidth;
                }

                ImGui::Button( category.c_str(), ImVec2(buttonWidth, 0.0f) );
                first                     = false;
            }
        }

        void  DrawCategoryTexts(const std::string& categories, float p_Width, bool p_AddSeparator = false)
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

        int   CalculateCategoryLineCount(const std::string& p_Categories, float p_Width)
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

        float CalculateWrappedTextHeight(const std::string& p_Text, float p_Width, float p_Padding = PADDING)
        {
            const ImGuiStyle& style = ImGui::GetStyle();
            const float textWidth   = p_Width - style.ChildBorderSize * 2.0f - p_Padding * 2.0f;
            const ImVec2 textSize   = ImGui::GetFont()->CalcTextSizeA( ImGui::GetFontSize(), FLT_MAX, textWidth, p_Text.c_str() );

            return textSize.y + p_Padding * 2.0f + style.ChildBorderSize * 2.0f;
        }

        float CalculateQuestionDetailsHeight(const Question& p_Question, float p_Width)
        {
            const ImGuiStyle& style         = ImGui::GetStyle();
            const float childBorder         = style.ChildBorderSize;
            const float itemSpacingY        = style.ItemSpacing.y;
            const float questionInfoHeight  = CalculateWrappedTextHeight(p_Question.Text, p_Width);
            float categoriesHeight          = CalculateCategoryButtonsHeight(p_Question.Categories, p_Width);

            const size_t optionCount        = p_Question.Options.size();
            const size_t rowCount           = (optionCount + 2) / 3;
            float answerTableHeight         = 0.0f;
            if (rowCount > 0)
            {
                const float rowHeight       = ImGui::GetFrameHeight();
                answerTableHeight           = rowCount * rowHeight + (rowCount - 1) * style.CellPadding.y * 2.0f;
            }

            const float contentHeight       = categoriesHeight + itemSpacingY + questionInfoHeight + itemSpacingY + answerTableHeight;

            return contentHeight + style.WindowPadding.y * 2.0f + childBorder * 2.0f + PADDING;
        }

    } // namespace

    Application::Application()
    {
        if (!glfwInit())
        {
            std::cerr << "Failed to initialize GLFW\n";
            std::exit(-1);
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        m_Window = glfwCreateWindow(
            800,
            600,
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

        m_Bank.Categories = "Category1;Category2;Category3;Category4";
    }

    Application::~Application()
    {
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
            // -------------------------
            // New ImGui frame
            // -------------------------

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            // -------------------------
            // UI
            // -------------------------

            BeginDockspace("MyDockspace", "MainDockspace", true, ImGuiDockNodeFlags_NoTabBar);

            if (ImGui::BeginMenuBar())
            {
                if (ImGui::MenuItem("Adicionar Questão"))
                {
                    m_QuestionWindow = true;
                }

                ImGui::EndMenuBar();
            }

            QuestionCreation();

            ImGui::SetNextWindowDockID(ImGui::GetID("MyDockspace"), ImGuiCond_Once);
            ImGui::Begin("QBank");

            float categoriesHeight = CalculateCategoryButtonsHeight(m_Bank.Categories, ImGui::GetContentRegionAvail().x);
            ImGui::BeginChild("Categories", ImVec2(0, categoriesHeight));
            DrawCategoryButtons(m_Bank.Categories, ImGui::GetContentRegionAvail().x);
            ImGui::EndChild();

            for (size_t i = 0; i < m_Bank.Questions.size(); ++i)
            {
                auto& question = m_Bank.Questions[i];
                ImGui::PushID((int)i);

                if (ImGui::TreeNodeEx(("Questão " + std::to_string(i + 1)).c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_Bullet))
                {
                    float questionDetailsHeight  = CalculateQuestionDetailsHeight(question, ImGui::GetContentRegionAvail().x);
                    ImGui::BeginChild("QuestionDetails", ImVec2(0, questionDetailsHeight), true);
                    {
                        DrawCategoryButtons(question.Categories, ImGui::GetContentRegionAvail().x);

                        float questionInfoHeight = CalculateWrappedTextHeight(question.Text, ImGui::GetContentRegionAvail().x, PADDING);
                        ImGui::BeginChild("QuestionInfo", ImVec2(0, questionInfoHeight), true);

                        ImGui::SetCursorPos(ImVec2(PADDING, PADDING));
                        ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x - PADDING);
                        ImGui::TextUnformatted(question.Text.c_str());
                        ImGui::PopTextWrapPos();

                        ImGui::EndChild();

                        if (ImGui::BeginTable("AnswerOptions", 3, ImGuiTableFlags_SizingStretchProp))
                        {
                            ImGui::TableSetupColumn("A", ImGuiTableColumnFlags_WidthStretch);
                            ImGui::TableSetupColumn("B", ImGuiTableColumnFlags_WidthStretch);
                            ImGui::TableSetupColumn("C", ImGuiTableColumnFlags_WidthStretch);

                            const size_t optionCount = question.Options.size();

                            for (size_t i = 0; i < optionCount; i += 3)
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

                    if (question.OptionMarkedIndex != -1 && question.OptionMarkedIndex != question.CorrectOptionIndex)
                    {
                        float height = CalculateWrappedTextHeight(question.Explanation, ImGui::GetContentRegionAvail().x, PADDING);
                        ImGui::BeginChild("Explanation", ImVec2(0, height), true);

                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));

                        ImGui::SetCursorPos(ImVec2(PADDING, PADDING));

                        ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x - PADDING);
                        ImGui::TextUnformatted(question.Explanation.c_str());
                        ImGui::PopTextWrapPos();

                        ImGui::PopStyleColor();

                        ImGui::EndChild();
                    }

                    ImGui::TreePop();
                }

                ImGui::PopID();
            }

            ImGui::End();

            EndDockspace();

            // -------------------------
            // Render
            // -------------------------

            int width, height;
            glfwGetFramebufferSize(m_Window, &width, &height);

            glViewport(0, 0, width, height);

            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            ImGuiIO& io    = ImGui::GetIO();
            io.DisplaySize = ImVec2((float)width, (float)height);

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
            {
                GLFWwindow* backup_current_context = glfwGetCurrentContext();

                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();

                glfwMakeContextCurrent(backup_current_context);
            }

            glfwSwapBuffers(m_Window);
            glfwPollEvents();
        }
    }

    void Application::QuestionCreation()
    {
        if (!m_QuestionWindow) return;

        static Question newQuestion       = {};
        static char questionText[1024]    = {};
        static char explanationText[1024] = {};
        static char answerText[256] = {};
        ImGui::Begin("##QuestionEditor", &m_QuestionWindow, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);

        ImGui::BeginChild("##Child", ImVec2(0, ImGui::GetContentRegionAvail().y - 23.0f), true);
        const float width                 = ImGui::GetContentRegionAvail().x;

        ImGui::Text("Texto da Questão:");
        ImGui::InputTextMultiline("##Text", questionText, sizeof(questionText), ImVec2(width, 50.0f), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_WordWrap);
        ImGui::Text("Explicação:");
        ImGui::InputTextMultiline("##Explanation", explanationText, sizeof(explanationText), ImVec2(width, 50.0f), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_WordWrap);

        float lineCount                   = std::max(CalculateCategoryLineCount(newQuestion.Categories, width), 1);
        float textHeight                  = lineCount * ImGui::GetTextLineHeight() + (lineCount - 1.0f + 4.0f) * ImGui::GetStyle().ItemSpacing.y;

        ImGui::BeginChild("##NewQuestionCategories", { 0, textHeight }, true);
        DrawCategoryTexts(newQuestion.Categories, width, true);
        ImGui::EndChild();

        if (ImGui::Button("Adicionar Categoria"))
            ImGui::OpenPopup("CategorySelector");

        if (ImGui::BeginPopup("CategorySelector"))
        {
            static char buffer[128]      = {};
            static char newCategory[128] = {};

            ImGui::Text("Search:");
            ImGui::InputText("##Search", buffer, sizeof(buffer), ImGuiInputTextFlags_AutoSelectAll);

            auto size = ImGui::GetContentRegionAvail();
            ImGui::SetNextWindowSizeConstraints({ size.x, 50.0f }, { size.x, 150.0f });
            ImGui::BeginChild("##Categories", { 0, 0 }, true);
            {
                std::stringstream ss(m_Bank.Categories);
                std::string category;
                while (std::getline(ss, category, ';'))
                {
                    if (buffer[0] != '\0')
                    {
                        std::string nameLower = category;
                        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

                        char bufferLower[128];
                        strncpy_s(bufferLower, buffer, sizeof(bufferLower));
                        std::transform(bufferLower, bufferLower + sizeof(bufferLower), bufferLower, ::tolower);

                        if (nameLower.find(bufferLower) == std::string::npos)
                            continue; // Skip if the name does not match the search
                    }

                    bool hasCategory                    = newQuestion.Categories.find(category) != std::string::npos;
                    if (ImGui::Selectable(category.c_str(), hasCategory))
                    {
                        if (!hasCategory)
                        {
                            if (!newQuestion.Categories.empty())
                                newQuestion.Categories += ';';

                            newQuestion.Categories     += category;
                        }
                        else
                        {
                            std::stringstream categories(newQuestion.Categories);
                            std::string result;
                            std::string current;
                            while (std::getline(categories, current, ';'))
                            {
                                if (current == category)
                                    continue;
                                if (!result.empty()) result += ';';
                                result += current;
                            }
                            newQuestion.Categories = std::move(result);
                        }
                        buffer[0] = '\0';
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
            ImGui::EndChild();
            ImGui::BeginChild("##AddCategory", { 0, ImGui::GetFrameHeight() + 4 * ImGui::GetStyle().ItemSpacing.y }, true);
            if (ImGui::InputText("##Category", newCategory, sizeof(newCategory), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
            {
                std::string category(newCategory);

                if (!m_Bank.Categories.empty())
                    m_Bank.Categories += ';';

                m_Bank.Categories     += category;
                newCategory[0]         = '\0';
            }
            ImGui::SameLine();
            if (ImGui::Button(" + "))
            {
                std::string category(newCategory);

                if (!m_Bank.Categories.empty())
                    m_Bank.Categories += ';';

                m_Bank.Categories     += category;
                newCategory[0]         = '\0';
            }
            ImGui::EndChild();

            ImGui::EndPopup();
        }

        ImGui::BeginChild("##Answers", { 0, 0 }, true);

        if (ImGui::InputText("##NewAnswer", answerText, sizeof(answerText), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
        {
            if (newQuestion.Options.size() < 26)
            {
                std::string answer(answerText);
                newQuestion.Options.push_back(answer);
            }
            answerText[0] = '\0';
        }
        ImGui::SameLine();
        if (ImGui::Button(" + "))
        {
            if (newQuestion.Options.size() < 26)
            {
                std::string answer(answerText);
                newQuestion.Options.push_back(answer);
            }
            answerText[0] = '\0';
        }

        if (ImGui::BeginTable("AnswerOptions", 3, ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("A", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("B", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("C", ImGuiTableColumnFlags_WidthStretch);

            const size_t optionCount = newQuestion.Options.size();
            for (size_t i = 0; i < optionCount; i += 3)
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

                    bool marked = newQuestion.CorrectOptionIndex == index;
                    if (ImGui::Checkbox("", &marked))
                    {
                        newQuestion.CorrectOptionIndex = newQuestion.CorrectOptionIndex == index ? -1 : index;
                        if (newQuestion.CorrectOptionIndex != -1)
                            newQuestion.CorrectOptionText = newQuestion.Options[newQuestion.CorrectOptionIndex];
                    }
                    ImGui::SameLine();

                    if (marked)
                        ImGui::PushStyleColor(ImGuiCol_Text, { 0.0f, 1.0f, 0.0f, 1.0f });

                    ImGui::Text("(%c)", 'A' + static_cast<char>(optionIndex));
                    ImGui::SameLine();
                    ImGui::PushTextWrapPos();
                    ImGui::TextUnformatted(newQuestion.Options[optionIndex].c_str());
                    ImGui::PopTextWrapPos();

                    if (marked)
                        ImGui::PopStyleColor();

                    ImGui::PopID();
                }
            }
            ImGui::EndTable();
        }

        ImGui::EndChild();

        ImGui::EndChild();

        if (ImGui::Button("Cancelar", ImVec2(100, 0)))
        {
            questionText[0]                 = '\0';
            explanationText[0]              = '\0';
            answerText[0]                   = '\0';
            newQuestion                     = {};
            m_QuestionWindow                = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Salvar", ImVec2(100, 0)))
        {
            newQuestion.Text                = questionText;
            newQuestion.Explanation         = explanationText;
            questionText[0]                 = '\0';
            explanationText[0]              = '\0';
            answerText[0]                   = '\0';
            m_Bank.Questions.push_back(newQuestion);
            newQuestion = {};
            m_QuestionWindow = false;
        }

        ImGui::End();
    }

} // namespace QB
