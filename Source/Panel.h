#pragma once

// std
#include <string>



namespace QB
{
    class Panel
    {
        public:
            Panel(const std::string& p_Name, bool p_Active = true) : m_Name(p_Name), m_Active(p_Active) {}
            virtual ~Panel() = default;

            void SetActive(bool p_Active) { m_Active = p_Active; }

            virtual void OnImGui() {}
            virtual void OnUpdate() {}

            const std::string& GetName() const { return m_Name; }
            bool& IsActive() { return m_Active; }

        protected:
            bool m_Active = true;
            std::string m_Name;
    };


} // namespace QB