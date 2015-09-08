#include "frame.h"
#include "AppIcon.xpm"

const wxString Peili::APP_VER = "2015.9.8";

Peili::Peili(const wxString &title, wxString aP)
: wxFrame(NULL, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxBG_STYLE_PAINT)
{
    wxBusyCursor WaitCursor;
    SetIcon(wxIcon(AppIcon_xpm));
    arg_path = aP;
    dir_pixs = "\\\\SAMBADROID\\sdcard\\Pictures\\Twitter";
    drawn_cnt = 0;
    time_pix = 1000;
    time_dir = 60000;

    wxPanel *panel = new wxPanel(this);
    mirror = new wxPanel(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE);
    wxSizer *sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(mirror, 1, wxEXPAND);
    panel->SetSizer(sizer);

#ifndef NDEBUG
    int bars[5] = {295, 145, 145, 145, -1};
    CreateStatusBar(5)->SetStatusWidths(5, bars);
#endif

    Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(Peili::OnExit));
    mirror->Connect(mirror->GetId(), wxEVT_PAINT, wxPaintEventHandler(Peili::draw_pixs), NULL, this);
    mirror->Connect(mirror->GetId(), wxEVT_ERASE_BACKGROUND, wxEraseEventHandler(Peili::clear_mirror), NULL, this);
    timer_pix.Connect(timer_pix.GetId(), wxEVT_TIMER, wxTimerEventHandler(Peili::load_pix), NULL, this);
    //Connect(wxEVT_COMMAND_THREAD_UPDATE, wxThreadEventHandler(Peili::OnThreadUpdate));
    //Connect(wxEVT_COMMAND_THREAD_COMPLETED, wxThreadEventHandler(Peili::OnThreadCompletion));
    //Connect(wxEVT_THREAD_CLOSE, wxCloseEventHandler(Peili::OnClose));

    wxToolTip::SetDelay(200);
    wxToolTip::SetAutoPop(32700);
    wxToolTip::SetReshow(1);
}

void Peili::load_pixs()
{
    rng.seed(std::chrono::system_clock::now().time_since_epoch().count());
    wxDir dir(dir_pixs);
    if(!dir.IsOpened()) return;
    wxString res;
    pixs.Clear();
    bool found = dir.GetFirst(&res, "*.jpg");
    while(found)
    {
        pixs.Add(dir_pixs + "\\" + res);
        found = dir.GetNext(&res);
    }
#ifndef NDEBUG
    SetStatusText("Pixs: " + format_int(pixs.GetCount()), 1);
#endif
    wxTimerEvent te;
    load_pix(te);
}

void Peili::load_pix(wxTimerEvent &event)
{
    timer_pix.Stop();
    if(pixs.IsEmpty()) return;
    //DoStartThread();
    {
        int pix = rng() % pixs.GetCount();
        pic = wxImage(pixs[pix], wxBITMAP_TYPE_JPEG);
        mirror->Refresh();
    }
    timer_pix.Start(time_pix);
}

void Peili::draw_pixs(wxPaintEvent &event)
{
    wxBufferedPaintDC dc(mirror);
#ifndef NDEBUG
    SetStatusText("Images drawn: " + format_int(++drawn_cnt), 0);
    SetStatusText("", 2);
    SetStatusText("", 3);
#endif
    if(pic.IsOk())
    {
        int mirrorX, mirrorY;
        mirror->GetClientSize(&mirrorX, &mirrorY);
        float centerX = mirrorX * 0.5f;
        float centerY = mirrorY * 0.5f;
        float picX = pic.GetWidth() * 0.5f;
        float picY = pic.GetHeight() * 0.5f;
        int overX = centerX - picX, overY = centerY - picY;
        if(overX < 0 || overY < 0)
        {
            if(overX < overY)
            {
                float prop = centerX / picX;
                picX *= prop;
                picY *= prop;
                pic.Rescale(2 * picX, 2 * picY, wxIMAGE_QUALITY_BICUBIC);
            }
            else
            {
                float prop = centerY / picY;
                picX *= prop;
                picY *= prop;
                pic.Rescale(2 * picX, 2 * picY, wxIMAGE_QUALITY_BICUBIC);
            }
            overX = centerX - picX;
            overY = centerY - picY;
        }
        if(overX > 0)
        {
            int rangeX = rng() % (overX * 2);
            centerX = centerX - overX + rangeX;
#ifndef NDEBUG
            SetStatusText("X rand: " + format_int(rangeX), 2);
#endif
        }
        if(overY > 0)
        {
            int rangeY = rng() % (overY * 2);
            centerY = centerY - overY + rangeY;
#ifndef NDEBUG
            SetStatusText("Y rand: " + format_int(rangeY), 3);
#endif
        }
        dc.DrawBitmap(wxBitmap(pic), centerX - picX, centerY - picY, true);
    }
}

/*void Peili::DoStartThread()
{
    pix_loader = new Lataaja(this);
    if(pix_loader->Run() != wxTHREAD_NO_ERROR)
    {
        wxLogError("Can't create the thread!");
        delete pix_loader;
        pix_loader = NULL;
    }
}

wxThread::ExitCode Lataaja::Entry()
{
    while(!TestDestroy())
    {
        //wxQueueEvent(kehys, new wxThreadEvent(wxEVT_COMMAND_THREAD_UPDATE));
    }
    //wxQueueEvent(kehys, new wxThreadEvent(wxEVT_COMMAND_THREAD_COMPLETED));
    return (wxThread::ExitCode)0;
}

Lataaja::~Lataaja()
{
    wxCriticalSectionLocker enter(kehys->pix_loader_cs);
    kehys->pix_loader = NULL;
}

void Peili::OnThreadCompletion(wxThreadEvent&)
{
    wxMessageOutputDebug().Printf("MYFRAME: Lataaja exited!\n");
}

void Peili::OnThreadUpdate(wxThreadEvent&)
{
    wxMessageOutputDebug().Printf("MYFRAME: Lataaja update...\n");
}

void Peili::DoPauseThread()
{
    wxCriticalSectionLocker enter(pix_loader_cs);
    if(pix_loader)
    {
        if(pix_loader->Pause() != wxTHREAD_NO_ERROR)
            wxLogError("Can't pause the thread!");
    }
}

void Peili::OnClose(wxCloseEvent&)
{
    {
        wxCriticalSectionLocker enter(pix_loader_cs);
        if(pix_loader)
        {
            wxMessageOutputDebug().Printf("MYFRAME: deleting thread");
            if(pix_loader->Delete() != wxTHREAD_NO_ERROR)
                wxLogError("Can't delete the thread!");
        }
    }
    while(true)
    {
        {
            wxCriticalSectionLocker enter(pix_loader_cs);
            if(!pix_loader) break;
        }
        wxThread::This()->Sleep(1);
    }
    Destroy();
}*/

wxString format_float(float value)
{
    std::stringbuf buffer;
    std::ostream os (&buffer);
    os << value;
    return buffer.str();
}

wxString format_int(int value)
{
    std::stringbuf buffer;
    std::ostream os (&buffer);
    os << value;
    return buffer.str();
}

void Peili::OnExit(wxCloseEvent &event)
{
    timer_pix.Stop();
    Destroy();
}
