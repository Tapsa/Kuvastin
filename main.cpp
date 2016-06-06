#include "main.h"

IMPLEMENT_APP(Kuvastin)

bool Kuvastin::OnInit()
{
    Peili* window;
    wxImage::AddHandler(new wxJPEGHandler);
    wxString settings;
    wxArrayString paths, names;
    for(size_t a = 1; a < wxApp::argc; ++a)
    {
        if('-' == wxApp::argv[a][0]) settings = wxApp::argv[a];
        else if(':' == wxApp::argv[a][1]) paths.Add(wxApp::argv[a]);
        else names.Add(wxApp::argv[a]);
    }

    window = new Peili("Kuvastin " + Peili::APP_VER, paths, settings, names);
    SetTopWindow(window);
    window->SetSize(900, 720);
    window->Show(true);
    window->load_pixs();

    return true;
}
