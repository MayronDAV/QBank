#pragma once
#include "Panel.h"
#include "Application.h"


namespace QB
{
    class QuestionPanel : public Panel
    {
        public:
            QuestionPanel();
            ~QuestionPanel() override = default;

            void SetQuestionToEdit(int p_Index) { m_QuestionToEdit = p_Index;}
            int GetQuestionToEdit() const { return m_QuestionToEdit; }

            void OnImGui() override;

        private:
            int m_QuestionToEdit = -1;
    };


} // namespace QB