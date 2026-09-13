#pragma once

// std
#include <string>
#include <vector>


namespace QB
{
    enum class FileDialogResult
    {
        SUCCESS,
        FAILED,
        CANCEL
    };

    using FilterList = std::vector<std::pair<std::string, std::string>>; // { "Description", "*.ext" }

    class FileDialog
    {
        public:
            static FileDialogResult Open(const FilterList& p_FilterList, const std::string& p_DefaultPath, std::string& p_OutPath);
            static FileDialogResult OpenMultiple(const FilterList& p_FilterList, const std::string& p_DefaultPath, std::vector<std::string>& p_OutPaths);
            static FileDialogResult Save(const FilterList& p_FilterList, const std::string& p_DefaultPath, std::string& p_OutPath);
            static FileDialogResult PickFolder(const std::string& p_DefaultPath, std::string& p_OutPath);
    };

} // namespace QB