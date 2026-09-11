include "premake-dependencies.lua"

workspace "QBank"
    architecture "x64"
    startproject "QBank"
    outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
    
    configurations
    {
        "Debug",
        "Release"
    }

    IMGUI_GLFW   = "ON"
    IMGUI_OPENGL = "ON"

    filter "system:windows"
        systemversion "latest"
        multiprocessorcompile ("On")
    
        filter { "system:windows", "configurations:Release" }
            linkoptions { "/SUBSYSTEM:WINDOWS" }
    
project "QBank"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"

    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "Source/**.h",
        "Source/**.cpp"
    }

    includedirs
    {
        "Source",
        "%{IncludeDir.glad}",
        "%{IncludeDir.glfw}",
        "%{IncludeDir.imgui}",
        "%{IncludeDir.stb}"
    }

    links
    {
        "glad",
        "glfw",
        "imgui",
        "stb"
    }

    defines { "IMGUI_DEFINE_MATH_OPERATORS" }

    filter "system:windows"
        systemversion "latest"
        buildoptions { "/utf-8", "/Zc:char8_t-", "/wd4251" }
        defines
        {
            "QB_WINDOWS",
            "UNICODE", 
            "_UNICODE" 
        }

    filter "system:linux"
        pic "on"
        systemversion "latest"
        buildoptions { "-finput-charset=UTF-8", "-fexec-charset=UTF-8", "-fno-char8_t", "-Wno-effc++" }
        linkoptions { "-Wl,-rpath,$$ORIGIN" } 
        defines "QB_LINUX"

    filter "configurations:Debug"
        defines "QB_DEBUG"
        runtime "Debug"
        symbols "on"
        optimize "off"

    filter "configurations:Release"
        defines "QB_RELEASE"
        runtime "Release"
        symbols "off"
        optimize "full"

group "Thirdparty"
    include "Tools"
    include "Thirdparty/glad"
    include "Thirdparty/premake-glfw.lua"
    include "Thirdparty/premake-imgui.lua"
    include "Thirdparty/stb"
group ""
