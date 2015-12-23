#include "frame.h"
#include "AppIcon.xpm"

const wxString Peili::APP_VER = "2015.12.17";

Peili::Peili(const wxString &title, const wxArrayString &paths, const wxString &settings)
: wxFrame(NULL, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxBG_STYLE_PAINT)
{
    wxBusyCursor WaitCursor;
    SetIcon(wxIcon(AppIcon_xpm));
    allow_del = equal_mix = false;
    if(1 < settings.size())
    {
        if("-eqmix" == settings) equal_mix = true;
        else if("-emad" == settings) equal_mix = allow_del = true;
        else if("-autodel" == settings) allow_del = true;
    }
    dirs_pixs = paths;
    pic_types = "*.jpg";
    drawn_cnt = 0;
    time_pix = 1000;
    time_dir = 144000;
    full_screen = clean_mirror = unique = dupl_found = inspect = browsing = false;
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
    //mirror->Connect(mirror->GetId(), wxEVT_RIGHT_UP, wxMouseEventHandler(Peili::right_click), NULL, this);
    mirror->Connect(mirror->GetId(), wxEVT_CHAR, wxKeyEventHandler(Peili::keyboard), NULL, this);
    timer_pix.Connect(timer_pix.GetId(), wxEVT_TIMER, wxTimerEventHandler(Peili::load_pix), NULL, this);
    timer_dir.Connect(timer_dir.GetId(), wxEVT_TIMER, wxTimerEventHandler(Peili::load_dir), NULL, this);
    Connect(wxEVT_COMMAND_THREAD, wxThreadEventHandler(Peili::thread_done));

    wxToolTip::SetDelay(200);
    wxToolTip::SetAutoPop(32700);
    wxToolTip::SetReshow(1);
}

void Peili::load_pixs()
{
    rng.seed(std::chrono::system_clock::now().time_since_epoch().count());
    {
        wxCriticalSectionLocker enter(dir_loader_cs);
        size_t dirs_size = dirs_pixs.size();
        pixs.Clear();
        path_ranges = std::vector<size_t>(2 * dirs_size, 0);
        for(size_t i = 0; i < dirs_size; ++i)
        {
            path_ranges[i] = pixs.size();
            if(wxDir(dirs_pixs[i]).IsOpened())
            {
                wxDir::GetAllFiles(dirs_pixs[i], &pixs, pic_types);
                path_ranges[dirs_size + i] = pixs.size() - path_ranges[i];
            }
        }
    }
#ifndef NDEBUG
    SetStatusText("Pixs: " + format_int(pixs.GetCount()), 1);
#endif
    wxTimerEvent te;
    load_pix(te);
    timer_dir.Start(time_dir);
}

void Peili::load_dir(wxTimerEvent &event)
{
    timer_dir.Stop();
    load_pixs();
}

void Peili::load_pix(wxTimerEvent &event)
{
    timer_pix.Stop();
    if(pixs.IsEmpty()) return;
    load_begin = std::chrono::system_clock::now();
    mirror->Refresh();
    if(!unique && !inspect)
    {
        size_t rng_beg = 0, rng_cnt = pixs.GetCount();
        if(equal_mix)
        {
            size_t ranges_cnt = path_ranges.size() / 2, slot = rng() % ranges_cnt;
            rng_cnt = path_ranges[ranges_cnt + slot];
            rng_beg = path_ranges[slot];
        }
        filename = pixs[rng_beg + rng() % rng_cnt];
        load_image();
    }
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

/*void Peili::right_click(wxMouseEvent &event)
{
    timer_pix.Stop();
    unique_pixs.clear();
    pix = 0;
    unique = true;
    pixs.Sort();
    wxMkdir(arg_path + "\\bin");
    filename = pixs[0];
    load_image();
}*/

void Peili::keyboard(wxKeyEvent &event)
{
    switch(event.GetKeyCode())
    {
        case 'a':
        {
            if(!browsing)
            {
                inspect = true;
                inspect_file = shown_pixs.begin();
            }
            if(++inspect_file != shown_pixs.end())
            {
                filename = *inspect_file;
            }
            load_image();
            browsing = true;
        }
        break;
        case 'd':
        {
            if(browsing)
            {
                if(--inspect_file != shown_pixs.end())
                {
                    filename = *inspect_file;
                }
            }
            load_image();
        }
        break;
        case 'w':
        {
            inspect = browsing = false;
            load_pixs();
        }
        break;
        case 's':
        {
            inspect = true;
        }
        break;
        case ' ':
        {
            if(!inspect)
            {
                inspect = true;
                inspect_file = shown_pixs.begin();
                if(++inspect_file != shown_pixs.end())
                {
                    filename = *inspect_file;
                }
                browsing = true;
            }
            wxExecute("Explorer /select," + filename);
        }
        break;
    }
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
    wxImage pic;
    {
        wxCriticalSectionLocker enter(kehys->dir_loader_cs);
        {
            wxLogNull log; // Kill error popups.
            pic = wxImage(kehys->filename, wxBITMAP_TYPE_JPEG);
        }
    }
    if(pic.IsOk())
    {
        int mirrorX, mirrorY;
        kehys->mirror->GetClientSize(&mirrorX, &mirrorY);
        float centerX = mirrorX * 0.5f;
        float centerY = mirrorY * 0.5f;
        // Crop edges.
        {
            unsigned char val = pic.GetRed(0, 0), max_diff = 12;
            int width = pic.GetWidth(), height = pic.GetHeight(), density = 8;
            int crop_top = 0, crop_bottom = 0, crop_left = 0, crop_right = 0;
            for(int h = 0; h < height; ++h)
            for(int w = 0; w < width; w += density)
            {
                unsigned char red = pic.GetRed(w, h);
                if(abs(val - red) > max_diff)
                {
                    crop_top = h;
                    goto DONE_TOP;
                }
            }
DONE_TOP:
            for(int w = 0; w < width; ++w)
            for(int h = crop_top; h < height; h += density)
            {
                unsigned char red = pic.GetRed(w, h);
                if(abs(val - red) > max_diff)
                {
                    crop_left = w;
                    goto DONE_LEFT;
                }
            }
DONE_LEFT:
            val = pic.GetRed(--width, --height);
            for(int h = height; h > 0; --h)
            for(int w = width; w > 0; w -= density)
            {
                unsigned char red = pic.GetRed(w, h);
                if(abs(val - red) > max_diff)
                {
                    crop_bottom = height - h + 1;
                    goto DONE_BOTTOM;
                }
            }
DONE_BOTTOM:
            for(int w = width; w > 0; --w)
            for(int h = height - crop_bottom; h > 0; h -= density)
            {
                unsigned char red = pic.GetRed(w, h);
                if(abs(val - red) > max_diff)
                {
                    crop_right = width - w + 1;
                    goto DONE_RIGHT;
                }
            }
DONE_RIGHT:
            if(crop_top || crop_bottom || crop_left || crop_right)
            {
                wxSize crop_size = pic.GetSize();
                crop_size.DecBy(crop_left + crop_right, crop_top + crop_bottom);
                wxRect crop(crop_left, crop_top, crop_size.GetWidth(), crop_size.GetHeight());
                pic = pic.GetSubImage(crop);
            }
        }
        float picX = pic.GetWidth() * 0.5f;
        float picY = pic.GetHeight() * 0.5f;
        // Check if duplicate. Still broken.
        if(kehys->unique)
        {
            int gap = pic.GetWidth() * 0.0625f;
            std::string check = "";
            for(int w=0; w < pic.GetWidth(); w += gap)
            for(int h=0; h < pic.GetHeight(); h += gap)
            check += (unsigned char)(32 + int(float(pic.GetRed(w, h)) * 0.37f));
            if(kehys->unique_pixs.count(check))
            {
                kehys->dupl_found = true;
                wxString old_name = kehys->filename;
                int split = 1 + old_name.Find('\\', true);
                wxString new_name = old_name.Mid(0, split) + "bin\\" + old_name.Mid(split);
                wxCriticalSectionLocker enter(kehys->dir_loader_cs);
                wxRenameFile(old_name, new_name);
            }
            else
            {
                kehys->unique_pixs.insert(check);
            }
        }
        if(kehys->allow_del && ((picX < 480 && picY < 480) || picX < 320 || picY < 320))
        {
            wxCriticalSectionLocker enter(kehys->dir_loader_cs);
            wxRemoveFile(kehys->filename);
        }
        if(!kehys->unique || kehys->dupl_found)
        {
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
    if(inspect)
    {
        mirror->Refresh();
    }
    else if(unique)
    {
#ifndef NDEBUG
        SetStatusText("Duplicate check done for " + format_int(pix), 0);
#endif
        if(dupl_found)
        {
            mirror->Refresh();
            dupl_found = false;
        }
        if(++pix < pixs.GetCount())
        {
            filename = pixs[pix];
            load_image();
        }
        else
        {
            unique = false;
            load_pixs();
        }
    }
    else
    {
        size_t load_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - load_begin).count();
        shown_pixs.emplace_front(filename);
        timer_pix.Start(load_time < time_pix ? time_pix - load_time : 0);
        if(shown_pixs.size() > 0xFF)
        {
            shown_pixs.pop_back();
        }
    }
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
