#include "FileDialog.h"

#if defined(QB_WINDOWS)
#include <Windows.h>
#include <commdlg.h>
#include <shlobj.h>
#elif defined(QB_LINUX)
// lib
#include <gtk/gtk.h>

// std
#include <sstream>
#include <iostream>
#endif


namespace QB
{
    namespace
    {
        std::vector<char> BuildFilter(const FilterList& p_Filters)
        {
            if (p_Filters.empty())
                return {};

            std::vector<char> buffer;

            for (const auto& [name, pattern] : p_Filters)
            {
                buffer.insert(buffer.end(), name.begin(), name.end());
                buffer.push_back('\0');

                buffer.insert(buffer.end(), pattern.begin(), pattern.end());
                buffer.push_back('\0');
            }

            buffer.push_back('\0'); // double null terminator

            return buffer;
        }

#if defined(QB_WINDOWS)
        static FileDialogResult OpenImpl(const FilterList& p_FilterList, const std::string& p_DefaultPath, std::string& p_OutPath)
        {
            OPENFILENAMEA ofn;
            char szFile[260] = { 0 };

            auto filterList = BuildFilter(p_FilterList);

            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = nullptr;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFilter = filterList.empty() ? "All Files\0*.*\0\0" : filterList.data();
            ofn.nFilterIndex = 1;
            ofn.lpstrInitialDir = p_DefaultPath.empty() ? nullptr : p_DefaultPath.c_str();
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

            if (GetOpenFileNameA(&ofn) == TRUE)
            {
                p_OutPath = ofn.lpstrFile;
                return FileDialogResult::SUCCESS;
            }

            return FileDialogResult::CANCEL;
        }

        static FileDialogResult OpenMultipleImpl(const FilterList& p_FilterList, const std::string& p_DefaultPath, std::vector<std::string>& p_OutPaths)
        {
            OPENFILENAMEA ofn;
            char szFile[1024] = { 0 };

            auto filterList = BuildFilter(p_FilterList);

            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = nullptr;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFilter = filterList.empty() ? "All Files\0*.*\0\0" : filterList.data();
            ofn.nFilterIndex = 1;
            ofn.lpstrInitialDir = p_DefaultPath.empty() ? nullptr : p_DefaultPath.c_str();
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

            if (GetOpenFileNameA(&ofn) == TRUE)
            {
                char* p = szFile;
                std::string directory = p;
                p += directory.size() + 1;

                while (*p)
                {
                    std::string file = p;
                    p_OutPaths.push_back(directory + "\\" + file);
                    p += file.size() + 1;
                }

                if (p_OutPaths.empty())
                    p_OutPaths.push_back(szFile);

                return FileDialogResult::SUCCESS;
            }

            return FileDialogResult::CANCEL;
        }

        static FileDialogResult SaveImpl(const FilterList& p_FilterList, const std::string& p_DefaultPath, std::string& p_OutPath)
        {
            OPENFILENAMEA ofn;
            char szFile[260] = { 0 };

            auto filterList = BuildFilter(p_FilterList);

            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = nullptr;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFilter = filterList.empty() ? "All Files\0*.*\0\0" : filterList.data();
            ofn.nFilterIndex = 1;
            ofn.lpstrInitialDir = p_DefaultPath.empty() ? nullptr : p_DefaultPath.c_str();
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;

            if (GetSaveFileNameA(&ofn) == TRUE)
            {
                p_OutPath = ofn.lpstrFile;
                return FileDialogResult::SUCCESS;
            }

            return FileDialogResult::CANCEL;
        }

        static FileDialogResult PickFolderImpl(const std::string& p_DefaultPath, std::string& p_OutPath)
        {
            BROWSEINFOA bi;
            ZeroMemory(&bi, sizeof(bi));
            bi.hwndOwner = nullptr;
            bi.pszDisplayName = new char[MAX_PATH];
            bi.lpszTitle = "Select a folder";
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;

            if (!p_DefaultPath.empty())
            {
                bi.lParam = reinterpret_cast<LPARAM>(p_DefaultPath.c_str());
                bi.lpfn = [](HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
                {
                    if (uMsg == BFFM_INITIALIZED)
                        SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
                    return 0;
                };
            }

            LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
            if (pidl != nullptr)
            {
                char path[MAX_PATH];
                if (SHGetPathFromIDListA(pidl, path))
                {
                    p_OutPath = path;
                    delete[] bi.pszDisplayName;
                    return FileDialogResult::SUCCESS;
                }

                CoTaskMemFree(pidl);
            }

            delete[] bi.pszDisplayName;
            return FileDialogResult::CANCEL;
        }
#elif defined(QB_LINUX)
        static std::vector<std::string> SplitPatterns(const std::string& p_PatternList)
        {
            std::vector<std::string> result;
            std::stringstream ss(p_PatternList);
            std::string item;

            while (std::getline(ss, item, ';'))
            {
                if (!item.empty())
                    result.push_back(item);
            }

            return result;
        }

        static void ApplyFilters(GtkFileChooser* p_Dialog, const FilterList& p_Filters)
        {
            for (const auto& [name, patternList] : p_Filters)
            {
                GtkFileFilter* filter = gtk_file_filter_new();
                gtk_file_filter_set_name(filter, name.c_str());

                auto patterns = SplitPatterns(patternList);

                for (const auto& pattern : patterns)
                {
                    if (pattern == "*.*")
                        gtk_file_filter_add_pattern(filter, "*");
                    else
                        gtk_file_filter_add_pattern(filter, pattern.c_str());
                }

                gtk_file_chooser_add_filter(p_Dialog, filter);
            }
        }

        static FileDialogResult OpenImpl(const FilterList& p_FilterList, const std::string& p_DefaultPath, std::string& p_OutPath)
        {
            FileDialogResult result = FileDialogResult::CANCEL;

            if (!gtk_init_check(nullptr, nullptr)) {
                std::cerr << "GTK initialization failed!";
                return FileDialogResult::FAILED;
            }

            GtkWidget* dialog = gtk_file_chooser_dialog_new(
                "Open File",
                nullptr,
                GTK_FILE_CHOOSER_ACTION_OPEN,
                "_Cancel", GTK_RESPONSE_CANCEL,
                "_Open", GTK_RESPONSE_ACCEPT,
                nullptr);

            ApplyFilters(GTK_FILE_CHOOSER(dialog), p_FilterList);

            if (!p_DefaultPath.empty())
                gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), p_DefaultPath.c_str());

            gint response = gtk_dialog_run(GTK_DIALOG(dialog));
            if (response == GTK_RESPONSE_ACCEPT)
            {
                char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
                p_OutPath = filename;
                g_free(filename);
                result = FileDialogResult::SUCCESS;
            }

            while (g_main_context_pending(nullptr))
            {
                g_main_context_iteration(nullptr, FALSE);
            }

            gtk_widget_destroy(dialog);

            for (int i = 0; i < 5; ++i)
            {
                while (g_main_context_pending(nullptr))
                {
                    g_main_context_iteration(nullptr, FALSE);
                }
            }

            return result;
        }

        static FileDialogResult OpenMultipleImpl(const FilterList& p_FilterList, const std::string& p_DefaultPath, std::vector<std::string>& p_OutPaths)
        {
            FileDialogResult result = FileDialogResult::CANCEL;

            if (!gtk_init_check(nullptr, nullptr))
            {
                std::cerr << "Failed to initialize GTK!";
                return FileDialogResult::FAILED;
            }

            GtkWidget* dialog = gtk_file_chooser_dialog_new(
                "Open Files",
                nullptr,
                GTK_FILE_CHOOSER_ACTION_OPEN,
                "Cancel", GTK_RESPONSE_CANCEL,
                "Open", GTK_RESPONSE_ACCEPT,
                nullptr);

            ApplyFilters(GTK_FILE_CHOOSER(dialog), p_FilterList);

            if (!p_DefaultPath.empty())
                gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), p_DefaultPath.c_str());

            gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);

            if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
            {
                GSList* filenames = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));
                for (GSList* iter = filenames; iter != nullptr; iter = iter->next)
                {
                    p_OutPaths.push_back(static_cast<char*>(iter->data));
                    g_free(iter->data);
                }
                g_slist_free(filenames);
                result = FileDialogResult::SUCCESS;
            }

            gtk_widget_destroy(dialog);

            return result;
        }

        static FileDialogResult SaveImpl(const FilterList& p_FilterList, const std::string& p_DefaultPath, std::string& p_OutPath)
        {
            FileDialogResult result = FileDialogResult::CANCEL;

            if (!gtk_init_check(nullptr, nullptr))
            {
                std::cerr << "Failed to initialize GTK!";
                return FileDialogResult::FAILED;
            }

            GtkWidget* dialog = gtk_file_chooser_dialog_new(
                "Save File",
                nullptr,
                GTK_FILE_CHOOSER_ACTION_SAVE,
                "Cancel", GTK_RESPONSE_CANCEL,
                "Save", GTK_RESPONSE_ACCEPT,
                nullptr);

            ApplyFilters(GTK_FILE_CHOOSER(dialog), p_FilterList);

            if (!p_DefaultPath.empty())
                gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), p_DefaultPath.c_str());

            if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
            {
                char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
                p_OutPath = filename;
                g_free(filename);
                result = FileDialogResult::SUCCESS;
            }

            gtk_widget_destroy(dialog);

            return result;
        }

        static FileDialogResult PickFolderImpl(const std::string& p_DefaultPath, std::string& p_OutPath)
        {
            FileDialogResult result = FileDialogResult::CANCEL;

            if (!gtk_init_check(nullptr, nullptr))
            {
                std::cerr << "Failed to initialize GTK!";
                return FileDialogResult::FAILED;
            }

            GtkWidget* dialog = gtk_file_chooser_dialog_new(
                "Select a Folder",
                nullptr,
                GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                "Cancel", GTK_RESPONSE_CANCEL,
                "Select", GTK_RESPONSE_ACCEPT,
                nullptr);

            if (!p_DefaultPath.empty())
                gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), p_DefaultPath.c_str());

            if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
            {
                char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
                p_OutPath = filename;
                g_free(filename);
                result = FileDialogResult::SUCCESS;
            }

            gtk_widget_destroy(dialog);

            return result;
        }

#endif
    }

    FileDialogResult FileDialog::Open(const FilterList& p_FilterList, const std::string& p_DefaultPath, std::string& p_OutPath)
    {
        return OpenImpl(p_FilterList, p_DefaultPath, p_OutPath);
    }

    FileDialogResult FileDialog::OpenMultiple(const FilterList& p_FilterList, const std::string& p_DefaultPath, std::vector<std::string>& p_OutPaths)
    {
        return OpenMultipleImpl(p_FilterList, p_DefaultPath, p_OutPaths);
    }

    FileDialogResult FileDialog::Save(const FilterList& p_FilterList, const std::string& p_DefaultPath, std::string& p_OutPath)
    {
        return SaveImpl(p_FilterList, p_DefaultPath, p_OutPath);
    }

    FileDialogResult FileDialog::PickFolder(const std::string& p_DefaultPath, std::string& p_OutPath)
    {
        return PickFolderImpl(p_DefaultPath, p_OutPath);
    }

} // namespace QB