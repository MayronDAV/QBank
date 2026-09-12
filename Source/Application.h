#pragma once
// std
#include <string>
#include <vector>

// lib
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>


namespace QB
{
    struct Question
    {
        std::string Text;
        std::vector<std::string> Options;
        int CorrectOptionIndex = -1;
        std::string CorrectOptionText;
        std::string Explanation;
        std::string Categories;

        int OptionMarkedIndex = -1;
    };

    struct Bank
    {
        std::string Name;
        std::string Categories;
        std::vector<Question> Questions;
    };

    class Application
    {
        public:
            Application();
            ~Application();

            void Run();

        private:
            void QuestionCreation();

        private:
            GLFWwindow* m_Window = nullptr;

            Bank m_Bank;
            bool m_QuestionWindow = false;
    };

} // namespace QB