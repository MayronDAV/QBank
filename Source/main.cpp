#include "Application.h"


int main()
{
    auto app = new QB::Application();
    app->Run();
    delete app;
}