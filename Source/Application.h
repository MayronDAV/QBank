#pragma once
#include "Image.h"
#include "ExternalDragDrop.h"

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


    struct AppSession
    {
        bool Maximize = false;
        int Width     = 800;
        int Height    = 600;

        std::vector<std::string> Banks;
        std::string StartBank = "";
        std::string CategorySelected = "";
    };

    class Application
    {
        public:
            Application();
            Application(int p_Argc, char** p_Argv);
            ~Application();

            void Run();
        private:
            void Init(bool p_LoadStartBank = true);
            void QuestionWindow();
            void StatusWindow();
            void BankExportWindow();
            void MainWindow();
            void SideMenuWindow();
            void SetupDockspace();
            void BanksWindow();

            void ImportBank(const std::string& p_AbsPath, bool p_SaveToSession = true);
            void RemoveBank(const std::string& p_AbsPath);
            void ChangeBank(const std::string& p_AbsPath);

        private:
            void DrawCategoryButtons(const std::string& p_Categories, float p_Width);

        private:
            GLFWwindow* m_Window       = nullptr;

            bool m_QuestionWindow      = false;
            bool m_StatusWindow        = false;
            bool m_BankExportWindow    = false;
            bool m_BankCreateWindow    = false;

            AppSession m_Session       = {};

            std::unordered_map<std::string, Bank> m_Banks;
            Bank* m_Bank               = nullptr;

            std::vector<std::string> m_BankNames;

            std::string m_CategorySelected = "";
            std::vector<int> m_FilteredQuestions;

            std::shared_ptr<Image> m_WhiteImage = nullptr;
            ExternalDragDrop m_ExternalDragDrop;

            int m_QuestionToEdit = -1;

            bool m_ShowBanks = false;
    };

} // namespace QB