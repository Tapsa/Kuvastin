#include "frame.h"
#include "AppIcon.xpm"

const wxString Peili::APP_VER = "2016.2.16";
const wxString Peili::HOT_KEYS = "Shortcuts\nLMBx2 = Full screen\nMMB = Exit app\nRMB = Remove duplicates"
"\nA = Previous file\nD = Next file\nE = Next random file\nS = Pause show\nW = Continue show"
"\nSPACE = Show current file in folder\nB = Show status bar\nC = Count LMB clicks\nL = Change screenplay"
"\nM = Reload folders periodically\nR = Reload folders";
wxString filename;
wxBitmap bitmap;//, last_bitmap;
std::mt19937 rng;
int last_x1, last_y1, last_x2, last_y2;//, old_x1, old_y1;
//int last_box[4][4];

Peili::Peili(const wxString &title, const wxArrayString &paths, const wxString &settings) :
    wxFrame(0, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxBG_STYLE_PAINT),
    dirs_pixs(paths), time_pix(1000), time_dir(144000)
{
    wxBusyCursor WaitCursor;
    SetIcon(wxIcon(AppIcon_xpm));
    if(1 < settings.size())
    {
        if("-eqmix" == settings) equal_mix = true;
        else if("-emad" == settings) equal_mix = allow_del = true;
        else if("-autodel" == settings) allow_del = true;
    }

    wxPanel *panel = new wxPanel(this);
    sizer = new wxBoxSizer(wxVERTICAL);
    mirror = new wxPanel(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE);
    bar = new wxStatusBar(panel);
    sizer->Add(mirror, 1, wxEXPAND);
    sizer->Add(bar, 0, wxEXPAND);
    panel->SetSizer(sizer);

    int bars[] = {295, 145, 145, 145, -1};
    bar->SetFieldsCount(5, bars);
    bar->Show(false);

    Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(Peili::OnExit));
    mirror->Connect(mirror->GetId(), wxEVT_PAINT, wxPaintEventHandler(Peili::draw_pixs), 0, this);
    mirror->Connect(mirror->GetId(), wxEVT_ERASE_BACKGROUND, wxEraseEventHandler(Peili::clear_mirror), 0, this);
    mirror->Connect(mirror->GetId(), wxEVT_MIDDLE_DOWN, wxMouseEventHandler(Peili::middle_click), 0, this);
    mirror->Connect(mirror->GetId(), wxEVT_LEFT_DCLICK, wxMouseEventHandler(Peili::left_click), 0, this);
    mirror->Connect(mirror->GetId(), wxEVT_LEFT_DOWN, wxMouseEventHandler(Peili::left_down), 0, this);
    mirror->Connect(mirror->GetId(), wxEVT_LEFT_UP, wxMouseEventHandler(Peili::left_up), 0, this);
    //mirror->Connect(mirror->GetId(), wxEVT_RIGHT_UP, wxMouseEventHandler(Peili::right_click), 0, this);
    mirror->Connect(mirror->GetId(), wxEVT_CHAR, wxKeyEventHandler(Peili::keyboard), 0, this);
    timer_pix.Connect(timer_pix.GetId(), wxEVT_TIMER, wxTimerEventHandler(Peili::load_pix), 0, this);
    timer_dir.Connect(timer_dir.GetId(), wxEVT_TIMER, wxTimerEventHandler(Peili::load_dir), 0, this);
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
                wxDir::GetAllFiles(dirs_pixs[i], &pixs, "*.jpg");
                path_ranges[dirs_size + i] = pixs.size() - path_ranges[i];
            }
        }
    }
    bar->SetStatusText("Images in show: " + format_int(pixs.GetCount()), 1);
    wxTimerEvent te;
    load_pix(te);
    if(auto_dir_reload) timer_dir.Start(time_dir);
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
    if(!unique)
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
    bar->SetStatusText("Images shown: " + format_int(++drawn_cnt), 0);
    /*{
        wxCriticalSectionLocker lock(bitmap_cs);
        if(last_bitmap.IsOk())
        dc.DrawBitmap(last_bitmap, old_x1, old_y1, true);
    }
    dc.SetPen(*wxRED_PEN);
    dc.SetBrush(*wxRED_BRUSH);
    dc.DrawRectangle(last_box[0][0], last_box[0][1], last_box[0][2], last_box[0][3]);
    dc.DrawRectangle(last_box[1][0], last_box[1][1], last_box[1][2], last_box[1][3]);
    dc.DrawRectangle(last_box[2][0], last_box[2][1], last_box[2][2], last_box[2][3]);
    dc.DrawRectangle(last_box[3][0], last_box[3][1], last_box[3][2], last_box[3][3]);*/
    {
        wxCriticalSectionLocker lock(bitmap_cs);
        if(bitmap.IsOk())
        {
            //last_bitmap = bitmap; old_x1 = last_x1; old_y1 = last_y1;
            dc.DrawBitmap(bitmap, last_x1, last_y1, true);
        }
    }
    if(cnt_clicks)
    {
        dc.SetBrush(wxBrush(wxColour(240, 240, 240)));
        dc.DrawRectangle(0, 0, 120, 20);
        wxString lmb = "DOWN " + format_int(lmb_down) + ", UP " + format_int(lmb_up);
        dc.DrawText(lmb, 2, 2);
    }
}

void Peili::left_down(wxMouseEvent &event)
{
    ++lmb_down;
}

void Peili::left_up(wxMouseEvent &event)
{
    ++lmb_up;
}

void Peili::left_click(wxMouseEvent &event)
{
    full_screen = !full_screen;
    ShowFullScreen(full_screen);
}

void Peili::middle_click(wxMouseEvent &event)
{
    wxCloseEvent ce;
    OnExit(ce);
}

/*void Peili::right_click(wxMouseEvent &event)
{
    timer_dir.Stop();
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
            break;
        }
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
            break;
        }
        case 'w':
        {
            inspect = browsing = false;
            load_pixs();
            break;
        }
        case 's':
        {
            inspect = true;
            break;
        }
        case 'e':
        {
            inspect = true;
            wxTimerEvent te;
            load_pix(te);
            break;
        }
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
            break;
        }
        case 'b':
        {
            show_status_bar = !show_status_bar;
            bar->Show(show_status_bar);
            sizer->Layout();
            break;
        }
        case 'c':
        {
            cnt_clicks = !cnt_clicks;
            break;
        }
        case 'l':
        {
            shotgun = shotgun == Scene::ALL_OVER ? Scene::AVOID_LAST : Scene::ALL_OVER;
            break;
        }
        case 'm':
        {
            auto_dir_reload = !auto_dir_reload;
            if(auto_dir_reload) timer_dir.Start(time_pix);
            clean_mirror = true;
            break;
        }
        case 'r':
        {
            load_pixs();
            clean_mirror = true;
            break;
        }
    }
}

void Peili::load_image()
{
    pix_loader = new Lataaja(this);
    if(pix_loader->Run() != wxTHREAD_NO_ERROR)
    {
        delete pix_loader;
        pix_loader = 0;
    }
}

wxThread::ExitCode Lataaja::Entry()
{
    wxImage pic;
    wxString picname(filename);
    {
        wxCriticalSectionLocker enter(kehys->dir_loader_cs);
        {
            wxLogNull log; // Kill error popups.
            pic = wxImage(picname, wxBITMAP_TYPE_JPEG);
        }
    }
    if(pic.IsOk())
    {
        int mirror_width, mirror_height;
        kehys->mirror->GetClientSize(&mirror_width, &mirror_height);
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
        int img_width = pic.GetWidth();
        int img_height = pic.GetHeight();
        // Check if duplicate. Still broken.
        /*if(kehys->unique)
        {
            int gap = pic.GetWidth() * 0.0625f;
            std::string check = "";
            for(int w=0; w < pic.GetWidth(); w += gap)
            for(int h=0; h < pic.GetHeight(); h += gap)
            check += (unsigned char)(32 + int(float(pic.GetRed(w, h)) * 0.37f));
            if(kehys->unique_pixs.count(check))
            {
                kehys->dupl_found = true;
                wxString old_name = picname;
                int split = 1 + old_name.Find('\\', true);
                wxString new_name = old_name.Mid(0, split) + "bin\\" + old_name.Mid(split);
                wxCriticalSectionLocker enter(kehys->dir_loader_cs);
                wxRenameFile(old_name, new_name);
            }
            else
            {
                kehys->unique_pixs.insert(check);
            }
        }*/
        if(kehys->allow_del && ((img_width < 960 && img_height < 960) || img_width < 640 || img_height < 640))
        {
            wxCriticalSectionLocker enter(kehys->dir_loader_cs);
            wxRemoveFile(picname);
        }
        //if(!kehys->unique || kehys->dupl_found)
        {
            int xpos = 0, ypos = 0, leftover_width = mirror_width - img_width, leftover_height = mirror_height - img_height;
            if(leftover_width < 0 || leftover_height < 0)
            {
                if(leftover_width < leftover_height)
                {
                    float prop = float(mirror_width) / float(img_width);
                    img_width *= prop;
                    img_height *= prop;
                    pic.Rescale(img_width, img_height, wxIMAGE_QUALITY_BICUBIC);
                }
                else
                {
                    float prop = float(mirror_height) / float(img_height);
                    img_width *= prop;
                    img_height *= prop;
                    pic.Rescale(img_width, img_height, wxIMAGE_QUALITY_BICUBIC);
                }
                leftover_width = mirror_width - img_width;
                leftover_height = mirror_height - img_height;
            }
            switch(kehys->shotgun)
            {
                case Peili::Scene::ALL_OVER:
                {
                    xpos = leftover_width > 0 ? rng() % leftover_width : 0;
                    ypos = leftover_height > 0 ? rng() % leftover_height : 0;
                    break;
                }
                case Peili::Scene::AVOID_LAST:
                {
                    int box[4][4] =
                    {
                        {0, 0, std::max(0, last_x1 - img_width), leftover_height},
                        {last_x2, 0, std::max(0, mirror_width - last_x2 - img_width), leftover_height},
                        {box[0][2], 0, std::max(0, std::min(leftover_width, last_x2) - box[0][2]), std::max(0, last_y1 - img_height)},
                        {box[0][2], last_y2, box[2][2], std::max(0, mirror_height - last_y2 - img_height)}
                    };
                    //std::memcpy(last_box, box, 16 * sizeof(int));
                    //wxMessageBox(format_int(box[0][2])+"\n"+format_int(box[2][0]));
                    int props[4] = {box[0][2] * box[0][3], box[1][2] * box[1][3], box[2][2] * box[2][3], box[3][2] * box[3][3]};
                    int total = props[0] + props[1] + props[2] + props[3];
                    /*wxMessageBox("Boxes\nX left "+format_int(box[0][0])+", "+format_int(box[0][1])+" -> "+format_int(box[0][2])+", "+format_int(box[0][3])+
                    "\nX right "+format_int(box[1][0])+", "+format_int(box[1][1])+" -> "+format_int(box[1][2])+", "+format_int(box[1][3])+
                    "\nY up "+format_int(box[2][0])+", "+format_int(box[2][1])+" -> "+format_int(box[2][2])+", "+format_int(box[2][3])+
                    "\nY down "+format_int(box[3][0])+", "+format_int(box[3][1])+" -> "+format_int(box[3][2])+", "+format_int(box[3][3])+
                    "\n% left "+format_int(props[0])+"\n% right "+format_int(props[1])+"\n% up "+format_int(props[2])+"\n% down "+format_int(props[3]));*/
                    if(!total)
                    {
                        if(std::max(last_x1, mirror_width - last_x2) < std::max(last_y1, mirror_height - last_y2))
                        {
                            //wxMessageBox("No space, corner Y");
                            ypos = last_y1 > mirror_height - last_y2 ? 0 : leftover_height;
                            xpos = rng() % std::max(1, leftover_width);
                        }
                        else
                        {
                            //wxMessageBox("No space, corner X");
                            xpos = last_x1 > mirror_width - last_x2 ? 0 : leftover_width;
                            ypos = rng() % std::max(1, leftover_height);
                        }
                        break;
                    }
                    int area = rng() % total;
                    for(size_t i = 0; i < 4; ++i, area -= props[i])
                    {
                        if(area < props[i])
                        {
                            xpos = box[i][0] + rng() % box[i][2];
                            ypos = box[i][1] + rng() % box[i][3];
                            break;
                        }
                    }
                    break;
                }
            }
            last_x2 = xpos + img_width;
            last_y2 = ypos + img_height;
            {
                wxCriticalSectionLocker lock(kehys->bitmap_cs);
                last_x1 = xpos;
                last_y1 = ypos;
                bitmap = wxBitmap(pic);
            }
        }
    }
    wxQueueEvent(kehys, new wxThreadEvent());
    return (wxThread::ExitCode)0;
}

Lataaja::~Lataaja()
{
    wxCriticalSectionLocker enter(kehys->pix_loader_cs);
    kehys->pix_loader = 0;
}

void Peili::thread_done(wxThreadEvent &event)
{
    if(close)
    {
        wxCloseEvent quit(wxEVT_CLOSE_WINDOW);
        ProcessEvent(quit);
    }
    else if(inspect)
    {
        mirror->Refresh();
    }
    /*else if(unique)
    {
        bar->SetStatusText("Duplicate check done for " + format_int(pix), 0);
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
    }*/
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
    timer_dir.Stop();
    timer_pix.Stop();
    if(pix_loader && pix_loader->IsAlive())
    {
        if(event.CanVeto())
        {
            close = true;
            event.Veto();
            return;
        }
    }
    Destroy();
}
