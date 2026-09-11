project "glfw"
    kind "StaticLib"
    language "C"
    staticruntime "off"
    warnings "off"

    targetdir ("%{wks.location}/bin/" .. outputdir .. "/Thirdparty/%{prj.name}")
    objdir ("%{wks.location}/bin-int/" .. outputdir .. "/Thirdparty/%{prj.name}")

    files
    {
        "glfw/include/GLFW/glfw3.h",
        "glfw/include/GLFW/glfw3native.h",
        "glfw/src/internal.h",
        "glfw/src/platform.h",
        "glfw/src/platform.c",
        "glfw/src/mappings.h",
        "glfw/src/context.c",
        "glfw/src/init.c",
        "glfw/src/input.c",
        "glfw/src/monitor.c",

        "glfw/src/egl_context.c",
        "glfw/src/osmesa_context.c",

        "glfw/src/null_platform.h",
        "glfw/src/null_joystick.h",
        "glfw/src/null_joystick.c",
        "glfw/src/null_init.c",
        "glfw/src/null_monitor.c",
        "glfw/src/null_window.c",

        "glfw/src/vulkan.c",
        "glfw/src/window.c",
    }

    filter "system:linux"
        pic "on"
        systemversion "latest"
        
        files
        {
            "glfw/src/x11_platform.h",
            "glfw/src/xkb_unicode.h",
            "glfw/src/xkb_unicode.c",
            "glfw/src/x11_init.c",
            "glfw/src/x11_monitor.c",
            "glfw/src/x11_window.c",
            "glfw/src/posix_module.c",
            "glfw/src/posix_time.h",
            "glfw/src/posix_time.c",
            "glfw/src/posix_thread.h",
            "glfw/src/posix_thread.c",
            "glfw/src/posix_module.c",
            "glfw/src/glx_context.c",
            "glfw/src/linux_joystick.h",
            "glfw/src/linux_joystick.c",

            "glfw/src/posix_poll.h",
            "glfw/src/posix_poll.c",
        }

        links { "X11", "Xrandr", "Xinerama", "Xcursor", "Xi", "Xext", "xkbcommon", "dl", "pthread" }
        includedirs {
            "/usr/include",
            "/usr/include/X11",
            "/usr/include/X11/extensions",
            "/usr/include/xkbcommon"
        }

        defines
        {
            "_GLFW_X11"
        }

    filter "system:macosx"
        pic "on"
        systemversion "latest"

        files
        {
            "glfw/src/cocoa_time.h",
            "glfw/src/cocoa_time.c",
            "glfw/src/posix_thread.h",
            "glfw/src/posix_thread.c",
            "glfw/src/posix_module.c",
            "glfw/src/cocoa_platform.h",
            "glfw/src/cocoa_joystick.h",
            "glfw/src/cocoa_joystick.m",
            "glfw/src/cocoa_init.m",
            "glfw/src/cocoa_monitor.m",
            "glfw/src/cocoa_window.m",
            "glfw/src/nsgl_context.m"
        }

        links { "Cocoa", "IOKit", "CoreFoundation" }

        defines
        {
            "_GLFW_COCOA"
        }

    filter "system:windows"
        systemversion "latest"

        files
        {
            "glfw/src/win32_time.h",
            "glfw/src/win32_time.c",
            "glfw/src/win32_thread.h",
            "glfw/src/win32_thread.c",
            "glfw/src/win32_module.c",
            "glfw/src/win32_platform.h",
            "glfw/src/win32_joystick.h",
            "glfw/src/win32_joystick.c",
            "glfw/src/win32_init.c",
            "glfw/src/win32_monitor.c",
            "glfw/src/win32_window.c",
            "glfw/src/wgl_context.c"
        }

        links { "gdi32" }

        defines 
        { 
            "_GLFW_WIN32",
            "_CRT_SECURE_NO_WARNINGS",
            "UNICODE",
            "_UNICODE"
        }

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"
        optimize "off"

    filter "configurations:Release"
        runtime "Release"
        symbols "off"
        optimize "full"
