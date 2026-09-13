#pragma once
// std
#include <string>
#include <vector>
#include <array>

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

        int OptionMarkedIndex         = -1;
    };

    struct Bank
    {
        std::string Name = "";
        std::string Categories;
        std::vector<Question> Questions;
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
            void QuestionCreation();
            void StatusWindow();
            void BankExportWindow();

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
    };

} // namespace QB