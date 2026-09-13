#include "Application.h"

#if defined(QB_WINDOWS)
#include <Windows.h>
#endif


namespace QB
{
    int Main(int p_Argc, char** p_Argv)
    {
        auto app = new QB::Application();
        if (app)
        {
            app->Run();
            delete app;
        }

        return 0;
    }

} // namespace QB

#if defined(QB_WINDOWS) && defined(QB_RELEASE)
    int APIENTRY WinMain(_In_ HINSTANCE p_hInst, _In_opt_ HINSTANCE p_hInstPrev, _In_ PSTR p_cmdline, _In_ int p_cmdshow)
    {
        return QB::Main(__argc, __argv);
    }
#else

#if defined(QB_RELEASE) && defined(QB_LINUX)
    #include <cstdio>
#endif

int main(int p_Argc, char** p_Argv)
{
#if defined(QB_RELEASE) && defined(QB_LINUX)
    if (freopen("/dev/null", "w", stdout) == nullptr)
    {
        KTN_CORE_ERROR("Could not redirect stdout");
    }
    if (freopen("/dev/null", "w", stderr) == nullptr)
    {
        KTN_CORE_ERROR("Could not redirect sterr");
    }
#endif

    return QB::Main(p_Argc, p_Argv);
}
#endif