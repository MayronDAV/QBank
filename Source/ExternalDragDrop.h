#pragma once

// std
#include <filesystem>
#include <vector>


#if defined(QB_WINDOWS)
#define NOMINMAX 1
#include <windows.h>
#endif



struct GLFWwindow;

namespace QB
{
    class ExternalDragDrop
    {
        public:
            static constexpr const char* PayloadType = "QB_EXTERNAL_FILES";

        public:
            bool Initialize(GLFWwindow* p_Window);
            void Shutdown();

            void EndFrame();

            bool BeginSource();
            void EndSource();

            bool IsDragging() const
            {
                return m_Dragging;
            }

            const std::vector<std::filesystem::path>& GetPaths() const
            {
                return m_Paths;
            }

            GLFWwindow* GetWindow() const
            {
                return m_Window;
            }

        private:
            void ClearPaths() { m_Paths.clear(); }

    #if defined(QB_WINDOWS)
            void OnDragEnter(struct IDataObject* p_DataObject, unsigned long p_KeyState, POINTL p_Point, unsigned long* p_Effect);
            void OnDragOver(unsigned long p_KeyState, POINTL p_Point, unsigned long* p_Effect);
            void OnDragLeave();
            void OnDrop(struct IDataObject* p_DataObject, unsigned long p_KeyState, POINTL p_Point, unsigned long* p_Effect);
            bool ExtractFiles(struct IDataObject* p_DataObject, std::vector<std::filesystem::path>& p_OutFiles);

        private:
            class DropTarget;
            DropTarget* m_DropTarget = nullptr;

            bool m_OleInitialized    = false;
            bool m_Registered        = false;
    #endif

        private:
            GLFWwindow* m_Window = nullptr;

            std::vector<std::filesystem::path> m_Paths;

            bool m_Dragging             = false;
            bool m_StopAfterFrame       = false;
            bool m_ClearPathsAfterFrame = false;
    };
}