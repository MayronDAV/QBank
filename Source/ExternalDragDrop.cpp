#include "ExternalDragDrop.h"

#include <GLFW/glfw3.h>

#if defined(QB_WINDOWS)

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <windows.h>
#include <ole2.h>
#include <oleidl.h>
#include <shellapi.h>
#endif

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>


namespace QB
{
#if defined(QB_WINDOWS)
    class ExternalDragDrop::DropTarget final : public IDropTarget
    {
    public:
        explicit DropTarget(ExternalDragDrop* p_Owner)
            : m_Owner(p_Owner) {
        }

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID p_IID, void** p_Object) override
        {
            if (!p_Object)
                return E_POINTER;

            *p_Object = nullptr;
            if (p_IID == IID_IUnknown || p_IID == IID_IDropTarget)
            {
                *p_Object = static_cast<IDropTarget*>(this);

                AddRef();

                return S_OK;
            }

            return E_NOINTERFACE;
        }

        ULONG STDMETHODCALLTYPE AddRef() override
        {
            return ++m_RefCount;
        }

        ULONG STDMETHODCALLTYPE Release() override
        {
            ULONG count = --m_RefCount;

            if (count == 0)
                delete this;

            return count;
        }

        HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* p_DataObject, DWORD p_KeyState, POINTL p_Point, DWORD* p_Effect) override
        {
            m_Owner->OnDragEnter(p_DataObject, p_KeyState, p_Point, p_Effect);
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE DragOver(DWORD p_KeyState, POINTL p_Point, DWORD* p_Effect) override
        {
            m_Owner->OnDragOver(p_KeyState, p_Point, p_Effect);
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE DragLeave() override
        {
            m_Owner->OnDragLeave();
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE Drop(IDataObject* p_DataObject, DWORD p_KeyState, POINTL p_Point, DWORD* p_Effect)
        {
            m_Owner->OnDrop(p_DataObject, p_KeyState, p_Point, p_Effect);
            return S_OK;
        }

    private:
        ULONG m_RefCount = 1;

        ExternalDragDrop* m_Owner = nullptr;
    };
#endif

    bool ExternalDragDrop::Initialize(GLFWwindow* p_Window)
    {
        if (!p_Window)
            return false;

        m_Window = p_Window;

#if defined(QB_WINDOWS)
        HWND hwnd = glfwGetWin32Window(m_Window);
        if (!hwnd)
        {
            m_Window = nullptr;
            return false;
        }

        const HRESULT oleResult = OleInitialize(nullptr);
        if (FAILED(oleResult))
        {
            m_Window = nullptr;
            return false;
        }

        m_OleInitialized = true;
        m_DropTarget = new DropTarget(this);

        const HRESULT result = RegisterDragDrop(hwnd, static_cast<IDropTarget*>(m_DropTarget));

        if (FAILED(result))
        {
            m_DropTarget->Release();
            m_DropTarget = nullptr;

            OleUninitialize();
            m_OleInitialized = false;

            m_Window = nullptr;

            return false;
        }

        m_Registered = true;
#else
        m_Window = nullptr;
        return false;
#endif

        return true;
    }

    void ExternalDragDrop::Shutdown()
    {
#if defined(QB_WINDOWS)
        if (m_Registered && m_Window)
        {
            HWND hwnd = glfwGetWin32Window(m_Window);
            if (hwnd)
                RevokeDragDrop(hwnd);

            m_Registered = false;
        }

        if (m_DropTarget)
        {
            m_DropTarget->Release();
            m_DropTarget = nullptr;
        }

        if (m_OleInitialized)
        {
            OleUninitialize();
            m_OleInitialized = false;
        }
#endif

        m_Window = nullptr;

        m_Dragging = false;
        m_StopAfterFrame = false;
        m_ClearPathsAfterFrame = false;

        m_Paths.clear();
    }

    void ExternalDragDrop::EndFrame()
    {
        if (m_StopAfterFrame)
        {
            m_StopAfterFrame = false;
            m_Dragging = false;

            m_ClearPathsAfterFrame = true;
        }
        else if (m_ClearPathsAfterFrame)
        {
            m_ClearPathsAfterFrame = false;
            m_Paths.clear();
        }
    }

    bool ExternalDragDrop::BeginSource()
    {
        if (!m_Dragging)
            return false;

        if (!ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceExtern | ImGuiDragDropFlags_SourceNoPreviewTooltip))
        {
            return false;
        }

        ImGui::SetDragDropPayload(PayloadType, nullptr, 0);

        ImGui::BeginTooltip();

        if (m_Paths.empty())
        {
            ImGui::TextUnformatted("External file");
        }
        else if (m_Paths.size() == 1)
        {
            ImGui::Text("%s", m_Paths[0].filename().string().c_str());
        }
        else
        {
            ImGui::Text("%zu files", m_Paths.size());
        }

        ImGui::EndTooltip();

        return true;
    }

    void ExternalDragDrop::EndSource()
    {
        ImGui::EndDragDropSource();
    }


#if defined(QB_WINDOWS)
    bool ExternalDragDrop::ExtractFiles(IDataObject* p_DataObject, std::vector<std::filesystem::path>& p_OutFiles)
    {
        if (!p_DataObject)
            return false;

        FORMATETC format{};

        format.cfFormat = CF_HDROP;
        format.ptd = nullptr;
        format.dwAspect = DVASPECT_CONTENT;
        format.lindex = -1;
        format.tymed = TYMED_HGLOBAL;

        if (FAILED(p_DataObject->QueryGetData(&format)))
        {
            return false;
        }

        STGMEDIUM medium{};

        if (FAILED(p_DataObject->GetData(&format, &medium)))
        {
            return false;
        }

        HDROP hDrop = static_cast<HDROP>(medium.hGlobal);
        if (!hDrop)
        {
            ReleaseStgMedium(&medium);
            return false;
        }

        const UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);

        p_OutFiles.clear();
        p_OutFiles.reserve(count);

        for (UINT i = 0; i < count; ++i)
        {
            const UINT length = DragQueryFileW(hDrop, i, nullptr, 0);

            std::vector<wchar_t> buffer(length + 1);
            DragQueryFileW(hDrop, i, buffer.data(), length + 1);

            p_OutFiles.emplace_back(buffer.data());
        }

        ReleaseStgMedium(&medium);

        return !p_OutFiles.empty();
    }

    void ExternalDragDrop::OnDragEnter(IDataObject* p_DataObject, DWORD /*p_KeyState*/, POINTL /*p_Point*/, DWORD* p_Effect)
    {
        glfwFocusWindow(m_Window);

        std::vector<std::filesystem::path> files;

        if (!ExtractFiles(p_DataObject, files))
        {
            m_Dragging = false;
            m_Paths.clear();

            if (p_Effect)
                *p_Effect = DROPEFFECT_NONE;

            return;
        }

        m_Paths = std::move(files);

        m_Dragging = true;
        m_StopAfterFrame = false;

        if (p_Effect)
            *p_Effect = DROPEFFECT_COPY;
    }

    void ExternalDragDrop::OnDragOver(DWORD /*p_KeyState*/, POINTL /*p_Point*/, DWORD* p_Effect)
    {
        if (!m_Dragging)
        {
            if (p_Effect)
                *p_Effect = DROPEFFECT_NONE;

            return;
        }

        if (p_Effect)
            *p_Effect = DROPEFFECT_COPY;
    }

    void ExternalDragDrop::OnDragLeave()
    {
        m_Dragging = false;
        m_StopAfterFrame = false;

        m_Paths.clear();
    }

    void ExternalDragDrop::OnDrop(IDataObject* p_DataObject, DWORD /*p_KeyState*/, POINTL /*p_Point*/, DWORD* p_Effect)
    {
        std::vector<std::filesystem::path> files;

        const bool success = ExtractFiles(p_DataObject, files);

        if (!success)
        {
            m_Dragging = true;
            m_StopAfterFrame = true;

            if (p_Effect)
                *p_Effect = DROPEFFECT_NONE;

            return;
        }

        m_Paths = std::move(files);
        m_Dragging = true;
        m_StopAfterFrame = true;

        if (p_Effect)
            *p_Effect = DROPEFFECT_COPY;
    }
#endif

} // namespace QB