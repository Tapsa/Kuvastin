#include "frame.h"
#include "AppIcon.xpm"

const wxString Peili::APP_VER = "2015.9.8";

Peili::Peili(const wxString &title, wxString aP)
: wxFrame(NULL, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxBG_STYLE_PAINT)
{
    wxBusyCursor WaitCursor;
    SetIcon(wxIcon(AppIcon_xpm));
    arg_path = aP;
    dir_pixs = (arg_path.size() > 3) ? arg_path : "\\\\SAMBADROID\\sdcard\\Pictures\\Twitter";
    drawn_cnt = 0;
    time_pix = 1000;
    time_dir = 60000;
    full_screen = clean_mirror = false;
    last_click = std::chrono::system_clock::now();
    pix_loader = NULL;

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
    mirror->Connect(mirror->GetId(), wxEVT_MIDDLE_DOWN, wxMouseEventHandler(Peili::middle_click), NULL, this);
    mirror->Connect(mirror->GetId(), wxEVT_LEFT_UP, wxMouseEventHandler(Peili::left_click), NULL, this);
    timer_pix.Connect(timer_pix.GetId(), wxEVT_TIMER, wxTimerEventHandler(Peili::load_pix), NULL, this);
    Connect(wxEVT_COMMAND_THREAD, wxThreadEventHandler(Peili::thread_done));

    wxToolTip::SetDelay(200);
    wxToolTip::SetAutoPop(32700);
    wxToolTip::SetReshow(1);
}

void Peili::load_pixs()
{
    rng.seed(std::chrono::system_clock::now().time_since_epoch().count());
    wxDir dir(dir_pixs);
    if(!dir.IsOpened()) return;
    wxString types = "*.jpg";
    wxDir::GetAllFiles(dir_pixs, &pixs, types);
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
    load_image();
}

void Peili::draw_pixs(wxPaintEvent &event)
{
    wxBufferedPaintDC dc(mirror);
    if(clean_mirror)
    {
        dc.SetBackground(*wxBLACK_BRUSH);
        dc.Clear();
        clean_mirror = false;
    }
#ifndef NDEBUG
    SetStatusText("Images drawn: " + format_int(++drawn_cnt), 0);
#endif
    if(pix_loader && pix_loader->IsRunning()) return;
    if(pic.IsOk())
    {
        dc.DrawBitmap(pic, picX, picY, true);
    }
}

void Peili::left_click(wxMouseEvent &event)
{
    std::chrono::time_point<std::chrono::system_clock> click = std::chrono::system_clock::now();
    auto time_passed = std::chrono::duration_cast<std::chrono::milliseconds>(click - last_click).count();
    if(900 > time_passed)
    {
        full_screen = !full_screen;
        ShowFullScreen(full_screen);
    }
    else if(3000 > time_passed && time_passed < 1500)
    {
        load_pixs();
        clean_mirror = true;
    }
    last_click = click;
}

void Peili::middle_click(wxMouseEvent &event)
{
    wxCloseEvent ce;
    OnExit(ce);
}

void Peili::load_image()
{
    pix_loader = new Lataaja(this);
    if(pix_loader->Run() != wxTHREAD_NO_ERROR)
    {
        delete pix_loader;
        pix_loader = NULL;
    }
}

wxThread::ExitCode Lataaja::Entry()
{
    int pix = kehys->rng() % kehys->pixs.GetCount();
    {
        wxLogNull log; // Kill error popups.
        wxImage pic(kehys->pixs[pix], wxBITMAP_TYPE_JPEG);
        if(pic.IsOk())
        {
            int mirrorX, mirrorY;
            kehys->mirror->GetClientSize(&mirrorX, &mirrorY);
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
                int rangeX = kehys->rng() % (overX * 2);
                centerX = centerX - overX + rangeX;
            }
            if(overY > 0)
            {
                int rangeY = kehys->rng() % (overY * 2);
                centerY = centerY - overY + rangeY;
            }
            kehys->picX = centerX - picX;
            kehys->picY = centerY - picY;
            kehys->pic = wxBitmap(pic);
        }
    }
    wxQueueEvent(kehys, new wxThreadEvent());
    return (wxThread::ExitCode)0;
}

Lataaja::~Lataaja()
{
    wxCriticalSectionLocker enter(kehys->pix_loader_cs);
    kehys->pix_loader = NULL;
}

void Peili::thread_done(wxThreadEvent &event)
{
    mirror->Refresh();
    timer_pix.Start(time_pix);
}

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
