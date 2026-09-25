#include "Application.h"
#include "FileDialog.h"
#include "Clipboard.h"
#include "Utils.h"

// Panels
#include "Panels/ImagesPanel.h"
#include "Panels/QuestionPanel.h"
#include "Panels/BanksPanel.h"
#include "Panels/StatsPanel.h"

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


// TODO: Add an image compressor

namespace QB
{
    namespace
    {
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
                if (p_MenuBar)
                {
                    ImGui::SetNextWindowPos(viewport->WorkPos);
                    ImGui::SetNextWindowSize(viewport->WorkSize);
                }
                else
                {
                    ImGui::SetNextWindowPos(viewport->Pos);
                    ImGui::SetNextWindowSize(viewport->Size);
                }

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

        bool BeginWindowDragDropTarget()
        {
            ImGuiWindow* window     = ImGui::GetCurrentWindow();

            if (window->SkipItems)
                return false;

            ImGuiViewport* viewport = window->Viewport;

            if (viewport == nullptr)
                return false;

            const ImVec2 padding    = { 3.5f, 3.5f };

            const ImVec2 pos        = window->Pos - padding;
            const ImVec2 size       = window->Size + padding * 2.0f;

            const ImRect bb(pos, pos + size);

            return ImGui::BeginDragDropTargetViewport(viewport, &bb);
        }

        void EndWindowDragDropTarget()
        {
            ImGui::EndDragDropTarget();
        }

    } // namespace

    Application::Application()
    {
        s_Instance = this;

        Init();
    }

    Application::Application(int p_Argc, char** p_Argv)
    {
        s_Instance = this;

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

            for (auto& [name, panel] : m_Panels)
            {
                if (panel->IsActive())
                    panel->OnUpdate();
            }

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

            BeginDockspace("MyDockspace", "MainDockspace", true, ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_NoResizeX | ImGuiDockNodeFlags_NoResizeY);

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
                        m_Panels["QuestionPanel"]->SetActive(true);
                    }

                    if (ImGui::MenuItem("Imagens"))
                    {
                        m_Panels["ImagesPanel"]->SetActive(true);
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::MenuItem("Finalizar Tentativa", nullptr, false, m_Bank != nullptr))
                {
                    m_Bank->CurStatus              = {};
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
                    m_Panels["StatsPanel"]->SetActive(true);
                }

                ImGui::EndMenuBar();
            }

            SetupDockspace();

            SideMenuWindow();

            for (auto& [name, panel] : m_Panels)
            {
                if (panel->IsActive())
                    panel->OnImGui();
            }

            MainWindow();

            BankExportWindow();

            if (BeginWindowDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(ExternalDragDrop::PayloadType))
                {
                    const auto& files = Application::Get().GetExternalDragDrop().GetPaths();

                    std::string startPath = "";
                    for (const auto& file : files)
                    {
                        if (file.extension() != ".qbank")
                            continue;

                        auto path = file.lexically_normal().string();
                        ImportBank(path);

                        if (startPath.empty())
                            startPath = path;
                    }

                    if (!startPath.empty())
                        ChangeBank(startPath);
                }

                EndWindowDragDropTarget();
            }

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

        #define CREATE_PANEL(panel) m_Panels[#panel] = std::make_shared<panel>();

        m_Panels.clear();
        CREATE_PANEL(ImagesPanel);
        CREATE_PANEL(QuestionPanel);
        CREATE_PANEL(BanksPanel);
        CREATE_PANEL(StatsPanel);

        #undef CREATE_PANEL
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

                auto it = std::find_if(m_Session.Banks.begin(), m_Session.Banks.end(), [&](const auto& p_Path)
                {
                    return p_Path == path;
                });

                if (it == m_Session.Banks.end())
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
                auto path = std::filesystem::absolute(folder / (name + ".qbank")).lexically_normal().generic_string();
                m_Bank->Path = path;
                ExportQuestionBank(*m_Bank);

                auto it = std::find_if(m_Session.Banks.begin(), m_Session.Banks.end(), [&](const auto& p_Path)
                {
                    return p_Path == path;
                });

                if (it == m_Session.Banks.end())
                    m_Session.Banks.push_back(path);

                m_Session.StartBank     = path;

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
            float categoriesHeight = std::max(CalculateCategoryButtonsHeight(m_Bank->Categories, ImGui::GetContentRegionAvail().x), PADDING * 2.0f);
            ImGui::BeginChild("Categories", ImVec2(0, categoriesHeight));
            DrawCategoryButtons(m_Bank->Categories, ImGui::GetContentRegionAvail().x, m_CategorySelected, m_FilteredQuestions, true);
            ImGui::EndChild();

            if (m_FilteredQuestions.empty() && !m_CategorySelected.empty())
            {
                ImGui::Text("Não foi encontrado nenhum resultado para: '%s'", m_CategorySelected.c_str());
            }
            else
            {
                size_t size = m_FilteredQuestions.empty() ? m_Bank->Questions.size() : m_FilteredQuestions.size();

                int questionToDelete = -1;

                for (size_t i = 0; i < size; ++i)
                {
                    int index      = m_FilteredQuestions.empty() || size != m_FilteredQuestions.size() ? (int)i : m_FilteredQuestions[i];
                    auto& question = m_Bank->Questions[index];
                    ImGui::PushID((int)i);

                    bool open = ImGui::TreeNodeEx(("Questão " + std::to_string(i + 1)).c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_Bullet);
                    ImGui::SameLine();
                    if (ImGui::Button("Editar"))
                    {
                        As<Panel, QuestionPanel>(m_Panels["QuestionPanel"])->SetQuestionToEdit(m_FilteredQuestions.empty() ? (int)i : m_FilteredQuestions[i]);
                        m_Panels["QuestionPanel"]->SetActive(true);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Deletar"))
                    {
                        questionToDelete = index;
                    }

                    if (open)
                    {
                        float questionDetailsHeight = CalculateQuestionDetailsHeight(question, ImGui::GetContentRegionAvail().x);
                        questionDetailsHeight = question.Image != 0 ? questionDetailsHeight + QUESTION_IMAGE_HEIGHT : questionDetailsHeight;

                        ImGui::BeginChild("##QuestionDetails", ImVec2(0, questionDetailsHeight), true);
                        {
                            DrawCategoryButtons(question.Categories, ImGui::GetContentRegionAvail().x, m_CategorySelected, m_FilteredQuestions);

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

                if (questionToDelete >= 0)
                {
                    auto it = m_Bank->Questions.begin() + questionToDelete;
                    if (it != m_Bank->Questions.end())
                        m_Bank->Questions.erase(it);
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

        auto banksPanel = m_Panels["BanksPanel"]->IsActive();
        if (VerticalButton(text, ImVec2(buttonWidth, buttonHeight), banksPanel))
        {
            m_Panels["BanksPanel"]->SetActive(!banksPanel);
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

            ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);

            ImGui::DockBuilderSetNodeSize(dockspaceID, viewport->WorkSize);

            ImGuiID leftID;
            ImGuiID centerID;

            const float sidebarWidth = SIDEBAR_WIDTH;
            const float ratio        = sidebarWidth / viewport->WorkSize.x;

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

    float CalculateQuestionDetailsHeight(const Question& p_Question, float p_Width)
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

} // namespace QB
