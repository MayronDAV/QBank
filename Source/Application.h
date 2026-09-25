#pragma once
#include "Image.h"
#include "ExternalDragDrop.h"
#include "Panel.h"

// std
#include <string>
#include <vector>
#include <array>
#include <unordered_map>

// lib
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>


namespace QB
{
    struct Question
    {
        std::string Text              = "";
        std::string Explanation       = "";
        std::string Categories        = "";
        int CorrectOptionIndex        = -1;
        std::string CorrectOptionText = "";
        std::array<std::string, 26> Options;
        int CurrentOptionIndex        = 0;
        uint64_t Image                = 0;

        int OptionMarkedIndex         = -1;
    };

    struct Status
    {
        int CorrectAnswers;
        int TotalAnswers;
        std::vector<int> IncorrectQuestions;
    };

    struct Bank
    {
        std::string Path = "";
        std::string Categories;
        std::vector<Question> Questions;
        std::unordered_map<uint64_t, std::shared_ptr<Image>> Images;
        
        Status CurStatus = {};
    };

    struct ImageData
    {
        std::vector<uint8_t> Data;
        uint32_t Width;
        uint32_t Height;

        bool Empty() const
        {
            return Data.empty() || Width <= 0 || Height <= 0;
        }

        ImageData()
        {
            Width = Height = 0;
        }
    };

    struct AppSession
    {
        inline static constexpr size_t MAX_RECENT_IMAGES = 5ul;

        bool Maximize = false;
        int Width     = 800;
        int Height    = 600;

        std::vector<std::string> Banks;
        std::string StartBank = "";
        std::string CategorySelected = "";

        std::array<ImageData, MAX_RECENT_IMAGES> RecentImages;
        size_t RecentImagesIndex = 0;
    };

    float CalculateQuestionDetailsHeight(const Question& p_Question, float p_Width);

    class Application
    {
        public:
            Application();
            Application(int p_Argc, char** p_Argv);
            ~Application();

            void Run();

            Bank* GetCurrentBank() { return m_Bank; }
            ExternalDragDrop& GetExternalDragDrop() { return m_ExternalDragDrop; }
            std::shared_ptr<Image>& GetWhiteImage() { return m_WhiteImage; }

            std::shared_ptr<Image> GetClipboardImage();
            uint64_t LoadClipboardImage();
            uint64_t LoadClipboardImage(const std::shared_ptr<Image>& p_Image);

            void ImportBank(const std::string& p_AbsPath, bool p_SaveToSession = true);
            void RemoveBank(const std::string& p_AbsPath);
            void ChangeBank(const std::string& p_AbsPath);
            void RemoveRecentImage(size_t p_Index);
            void PromoteRecentImage(size_t p_Index);

            GLFWwindow* GetWindow() { return m_Window; }

            AppSession& GetSession() { return m_Session; }
            static Application& Get() { return *s_Instance; }

        private:
            void Init(bool p_LoadStartBank = true);
            void SetupDockspace();
            void BankExportWindow();
            void MainWindow();
            void SideMenuWindow();

        private:
            GLFWwindow* m_Window                  = nullptr;

            AppSession m_Session                  = {};

            std::unordered_map<std::string, Bank> m_Banks;
            Bank* m_Bank                          = nullptr;

            std::unordered_map<std::string, std::shared_ptr<Panel>> m_Panels;

            bool m_BankExportWindow               = false;
            bool m_BankCreateWindow               = false;

            std::vector<std::string> m_BankNames;

            std::string m_CategorySelected        = "";
            std::vector<int> m_FilteredQuestions;

            std::shared_ptr<Image> m_WhiteImage   = nullptr;
            ExternalDragDrop m_ExternalDragDrop;

            inline static Application* s_Instance = nullptr;
    };

} // namespace QB