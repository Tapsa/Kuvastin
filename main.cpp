#include "main.h"

IMPLEMENT_APP(Kuvastin)

bool Kuvastin::OnInit()
{
    Peili* window;
    wxImage::AddHandler(new wxJPEGHandler);
    wxString argPath = (wxApp::argc > 1) ? wxApp::argv[1] : "";

    window = new Peili("Kuvastin " + Peili::APP_VER, argPath);
    SetTopWindow(window);
    window->SetSize(900, 720);
    window->Show(true);
    window->load_pixs();

    return true;
}
