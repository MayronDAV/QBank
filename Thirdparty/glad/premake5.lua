project "glad"
    kind "StaticLib"
    language "C"
    staticruntime "on"
    
    targetdir ("%{wks.location}/bin/" .. outputdir .. "/Thirdparty/%{prj.name}")
    objdir ("%{wks.location}/bin-int/" .. outputdir .. "/Thirdparty/%{prj.name}")

    files
    {
        "include/glad/glad.h",
        "include/KHR/khrplatform.h",
        "src/glad.c"
    }

    includedirs
    {
        "include"
    }
    
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