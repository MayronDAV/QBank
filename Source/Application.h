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
        std::string Text;
        int CorrectOptionIndex = -1;
        std::string CorrectOptionText;
        std::string Explanation;
        std::string Categories;
        std::array<std::string, 26> Options;
        int CurrentOptionIndex = 0;

        int OptionMarkedIndex = -1;
    };

    struct Bank
    {
        std::string Name;
        std::string Categories;
        std::vector<Question> Questions;
    };

    struct Status
    {
        int CorrectAnswers;
        int TotalAnswers;
        std::vector<int> IncorrectQuestions;
    };

    class Application
    {
        public:
            Application();
            ~Application();

            void Run();
        private:
            void QuestionCreation();
            void StatusWindow();

        private:
            GLFWwindow* m_Window = nullptr;

            Bank m_Bank;
            Status m_Status;
            bool m_QuestionWindow = false;
            bool m_StatusWindow = false;
    };

} // namespace QB