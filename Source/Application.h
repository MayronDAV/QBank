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

    struct Bank
    {
        std::string Name = "";
        std::string Categories;
        std::vector<Question> Questions;
        std::unordered_map<uint64_t, std::shared_ptr<Image>> Images;
    };

    struct Status
    {
        int CorrectAnswers;
        int TotalAnswers;
        std::vector<int> IncorrectQuestions;
    };

    struct ApplicationConfig
    {
        std::string StartBank = "";
        bool Maximize         = false;
        int Width             = 800;
        int Height            = 600;
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

        private:
            void DrawCategoryButtons(const std::string& p_Categories, float p_Width);

        private:
            GLFWwindow* m_Window    = nullptr;

            Status m_Status;
            bool m_QuestionWindow   = false;
            bool m_StatusWindow     = false;
            bool m_BankExportWindow = false;

            struct WindowData
            {
                ApplicationConfig Config = {};
                Bank CurrentBank;

            } m_Data = {};

            std::vector<std::string> m_BankNames;

            std::string m_CategorySelected = "";
            std::vector<int> m_FilteredQuestions;

            std::shared_ptr<Image> m_WhiteImage = nullptr;
            ExternalDragDrop m_ExternalDragDrop;

            int m_QuestionToEdit = -1;
    };

} // namespace QB