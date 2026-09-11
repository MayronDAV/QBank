project "imgui"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "off"

    targetdir ("%{wks.location}/bin/" .. outputdir .. "/Thirdparty/%{prj.name}")
    objdir ("%{wks.location}/bin-int/" .. outputdir .. "/Thirdparty/%{prj.name}")

    
    files
    {
        "imgui/imconfig.h",
        "imgui/imgui.h",
        "imgui/imgui.cpp",
        "imgui/imgui_draw.cpp",
        "imgui/imgui_internal.h",
        "imgui/imgui_tables.cpp",
        "imgui/imgui_widgets.cpp",
        "imgui/imstb_rectpack.h",
        "imgui/imstb_textedit.h",
        "imgui/imstb_truetype.h",
        "imgui/imgui_demo.cpp"
    }

    includedirs
    {
        "imgui"
    }

    if IMGUI_GLFW == "ON" then
        files
        {
            "imgui/backends/imgui_impl_glfw.h",
            "imgui/backends/imgui_impl_glfw.cpp"
        }

        includedirs { "%{IncludeDir.glfw}", "imgui/backends" }
        links "glfw"
    end

    if IMGUI_OPENGL == "ON" then
        files
        {
            "imgui/backends/imgui_impl_opengl3_loader.h",
            "imgui/backends/imgui_impl_opengl3.h",
            "imgui/backends/imgui_impl_opengl3.cpp"
        }

        includedirs { "imgui/backends" }

        filter "system:windows"
            links { "opengl32.lib" }
        
        filter "system:linux"
            links { "GL" }
    end

    filter "system:windows"
        systemversion "latest"

    filter "system:linux"
        pic "on"
        systemversion "latest"

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"
        optimize "off"

    filter "configurations:Release"
        runtime "Release"
        symbols "off"
        optimize "full"
