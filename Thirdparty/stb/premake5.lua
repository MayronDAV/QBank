project "stb"
    kind "StaticLib"
    language "C"
    staticruntime "off"
    
    targetdir ("%{wks.location}/bin/" .. outputdir .. "/Thirdparty/%{prj.name}")
    objdir ("%{wks.location}/bin-int/" .. outputdir .. "/Thirdparty/%{prj.name}")

    files
    {
        "stb/*.h",
        "stb/stb_build.cpp"
    }

    includedirs
    {
        "stb"
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