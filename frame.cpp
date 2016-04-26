#include "frame.h"
#include "AppIcon.xpm"

const wxString Peili::APP_VER = "2016.4.24";
const wxString Peili::HOT_KEYS = "Shortcuts\nLMBx2 = Full screen\nMMB = Exit app\nRMB = Remove duplicates"
"\nA = Previous file\nD = Next file\nE = Next random file\nS = Pause show\nW = Continue show"
"\nSPACE = Show current file in folder\nB = Show status bar\nC = Count LMB clicks\nL = Change screenplay"
"\nM = Reload folders periodically\nR = Reload folders";
std::list<Nouto> fetches;
std::list<Nouto>::iterator fetch;
std::mt19937 rng, rngk;
int last_x1, last_y1, last_x2, last_y2, loading_pixs;
bool filter_by_time;
uint64_t recent_depth = 14;

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
    fetch = fetches.begin();

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

    Bind(wxEVT_CLOSE_WINDOW, &Peili::OnExit, this);
    mirror->Bind(wxEVT_PAINT, &Peili::draw_pixs, this);
    mirror->Bind(wxEVT_ERASE_BACKGROUND, &Peili::clear_mirror, this);
    mirror->Bind(wxEVT_MIDDLE_DOWN, &Peili::middle_click, this);
    mirror->Bind(wxEVT_LEFT_DCLICK, &Peili::left_click, this);
    mirror->Bind(wxEVT_LEFT_DOWN, &Peili::left_down, this);
    mirror->Bind(wxEVT_LEFT_UP, &Peili::left_up, this);
    //mirror->Bind(wxEVT_RIGHT_UP, &Peili::right_click, this);
    mirror->Bind(wxEVT_CHAR, &Peili::keyboard, this);
    timer_pix.Bind(wxEVT_TIMER, &Peili::load_pix, this);
    timer_dir.Bind(wxEVT_TIMER, &Peili::load_dir, this);
    timer_queue.Bind(wxEVT_TIMER, &Peili::load_merged, this);
    Bind(wxEVT_COMMAND_THREAD, &Peili::thread_done, this);

    wxToolTip::SetDelay(200);
    wxToolTip::SetAutoPop(32700);
    wxToolTip::SetReshow(1);
}

void Peili::load_pixs()
{
    wait_threads();
    {
        wxCriticalSectionLocker lock(fetch_cs);
        // Reset prefetched file buffer.
        fetches.erase(std::next(fetch, 1), fetches.end());
        working = true;
    }
    rng.seed(std::chrono::system_clock::now().time_since_epoch().count());
    rngk.seed(rng());
    {
        wxCriticalSectionLocker lock(folder_cs);
        ++loading_pixs;
        bar->SetStatusText("Days: " + format_int(recent_depth), 4);
        // Get matching file names of all supplied folders.
        size_t dirs_size = dirs_pixs.size();
        pixs.Clear();
        path_ranges = std::vector<size_t>(2 * dirs_size, 0);
        for(size_t i = 0; i < dirs_size; ++i)
        {
            path_ranges[i] = pixs.size();
            if(wxDir(dirs_pixs[i]).IsOpened())
            {
                if(filter_by_time)
                {
                    wxArrayString files;
                    wxDir::GetAllFiles(dirs_pixs[i], &files, "*.jpg");
                    for(const wxString &name: files)
                    {
                        wxDateTime created;
                        wxFileName(name).GetTimes(0, 0, &created);
                        if(wxDateTime::GetTimeNow() - created.GetTicks() > 28800 + 86400 * recent_depth) continue;
                        pixs.Add(name);
                    }
                }
                else
                {
                    wxDir::GetAllFiles(dirs_pixs[i], &pixs, "*.jpg");
                }
                path_ranges[dirs_size + i] = pixs.size() - path_ranges[i];
            }
        }
        --loading_pixs;
    }
    bar->SetStatusText("Images in show: " + format_int(pixs.GetCount()), 1);
    wxTimerEvent te;
    load_pix(te);
    if(auto_dir_reload) timer_dir.Start(time_dir);
}

void Peili::load_merged(wxTimerEvent &event)
{
    timer_queue.Stop();
    if(loading_pixs)
    {
        timer_queue.Start(400);
    }
    else
    {
        load_pixs();
        clean_mirror = true;
    }
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
    advance();
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
    bar->SetStatusText("Images shown: " + format_int(++drawn_cnt), 0);
    if(fetch != fetches.end() && fetch->file.IsOk())
    {
        dc.DrawBitmap(fetch->file, last_x1, last_y1, true);
    }
    if(cnt_clicks)
    {
        dc.SetBrush(wxBrush(wxColour(240, 240, 240)));
        dc.DrawRectangle(0, 0, 120, 20);
        wxString lmb = "DOWN " + format_int(lmb_down) + ", UP " + format_int(lmb_up);
        dc.DrawText(lmb, 2, 2);
    }
}

void Peili::advance()
{
    wxCriticalSectionLocker lock(fetch_cs);
    // Advance if there is next item ready. TODO: Wait until it is ready?
    if(std::next(fetch, 1) != fetches.end())
    {
        // Skip first item as it will be popped out.
        std::advance(fetch, fetch == fetches.end() ? 2 : 1);
        int trail = std::distance(fetches.begin(), fetch);
        bar->SetStatusText("Trail: " + format_int(trail), 2);
        if(trail > 0x2F)
        {
            fetches.erase(fetches.begin(), std::prev(fetch, 0x1F));
        }
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
    wait_threads();
    unique_pixs.clear();
    pix = 0;
    unique = true;
    pixs.Sort();
    wxMkdir(arg_path + "\\bin");
    filename = pixs[0];
    load_image();
}*/

void Peili::stop_flow()
{
    flow = false;
    timer_pix.Stop();
}

void Peili::keyboard(wxKeyEvent &event)
{
    switch(event.GetKeyCode())
    {
        case 'a':
        {
            stop_flow();
            if(std::prev(fetch, 1) != fetches.end()) --fetch;
            load_image();
            return;
        }
        case 'd':
        {
            if(std::next(fetch, 1) != fetches.end()) ++fetch;
            load_image();
            return;
        }
        case 'w':
        {
            flow = true;
            load_pixs();
            return;
        }
        case 's':
        {
            stop_flow();
            return;
        }
        case 'e':
        {
            timer_pix.Start(0);
            return;
        }
        case ' ':
        {
            stop_flow();
            if(fetch != fetches.end())
            {
                wxExecute("Explorer /select," + fetch->filename);
            }
            return;
        }
        case 'b':
        {
            show_status_bar = !show_status_bar;
            bar->Show(show_status_bar);
            sizer->Layout();
            return;
        }
        case 'c':
        {
            cnt_clicks = !cnt_clicks;
            return;
        }
        case 'l':
        {
            shotgun = shotgun == Scene::ALL_OVER ? Scene::AVOID_LAST : Scene::ALL_OVER;
            return;
        }
        case 'm':
        {
            auto_dir_reload = !auto_dir_reload;
            if(auto_dir_reload) timer_dir.Start(time_pix);
            clean_mirror = true;
            return;
        }
        case 't':
        {
            filter_by_time = !filter_by_time;
        }
        case 'r':
        {
            break;
        }
        case '-':
        {
            recent_depth >>= 1;
            break;
        }
        case '+':
        {
            recent_depth = recent_depth ? recent_depth << 1 : 1;
            break;
        }
        default: return;
    }
    timer_queue.Start(0);
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

wxThread::ExitCode Noutaja::Entry()
{
    for(size_t i = 0; kehys->working && i < 0x2F; ++i)
    {
        kehys->folder_cs.Enter();
        size_t rng_beg = 0, rng_cnt = kehys->pixs.GetCount();
        if(kehys->equal_mix)
        {
            size_t ranges_cnt = kehys->path_ranges.size() / 2, slot = rng() % ranges_cnt;
            rng_cnt = kehys->path_ranges[ranges_cnt + slot];
            rng_beg = kehys->path_ranges[slot];
        }
        if(0 == rng_cnt)
        {
            kehys->folder_cs.Leave();
            continue;
        }
        wxString picname(kehys->pixs[rng_beg + rngk() % rng_cnt]);

        wxLogNull log;
        wxImage pic(picname, wxBITMAP_TYPE_JPEG);
        kehys->folder_cs.Leave();
        if(!pic.IsOk()) continue;

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
        if(kehys->allow_del && ((img_width < 960 && img_height < 960) || img_width < 640 || img_height < 640))
        {
            wxCriticalSectionLocker lock(kehys->folder_cs);
            wxRemoveFile(picname);
        }
        int leftover_width = mirror_width - img_width, leftover_height = mirror_height - img_height;
        if(leftover_width < 0 || leftover_height < 0)
        {
            float prop = leftover_width < leftover_height ? (float) mirror_width / img_width : (float) mirror_height / img_height;
            img_width *= prop;
            img_height *= prop;
            pic.Rescale(img_width, img_height, wxIMAGE_QUALITY_BICUBIC);
            leftover_width = mirror_width - img_width;
            leftover_height = mirror_height - img_height;
        }
        fetches.emplace_back(pic, picname, mirror_width, mirror_height, img_width, img_height, leftover_width, leftover_height);
    }
    return wxThread::ExitCode(0xFE);
}

wxThread::ExitCode Lataaja::Entry()
{
    int buffer = std::distance(fetch, fetches.end());
    kehys->bar->SetStatusText("Buffer: " + format_int(buffer), 3);
    // Not prefetching and should prefetch more.
    if(!kehys->fetcher && buffer < 0x1F)
    {
        kehys->fetcher = new Noutaja(kehys);
        if(kehys->fetcher->Run() != wxTHREAD_NO_ERROR)
        {
            delete kehys->fetcher;
            kehys->fetcher = 0;
            return wxThread::ExitCode(0xE1);
        }
    }
    if(fetches.empty())
    {
        do
        {
            Sleep(200);
        }
        while(fetches.empty());
        fetch = fetches.begin();
    }
    if(fetch == fetches.end())
    {
        wxQueueEvent(kehys, new wxThreadEvent());
        return wxThread::ExitCode(0x4E20 + fetches.size());
    }
    const int mirror_width = fetch->mirror_width, mirror_height = fetch->mirror_height;
    const int img_width = fetch->img_width, img_height = fetch->img_height;
    const int leftover_width = fetch->leftover_width, leftover_height = fetch->leftover_height;
    int xpos = 0, ypos = 0;
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
            const int box[4][4] =
            {
                {0, 0, std::max(0, last_x1 - img_width + 1), leftover_height + 1},
                {last_x2, 0, std::max(0, mirror_width - last_x2 - img_width + 1), leftover_height + 1},
                {box[0][2], 0, std::max(0, std::min(leftover_width + 1, last_x2) - box[0][2]), std::max(0, last_y1 - img_height + 1)},
                {box[0][2], last_y2, box[2][2], std::max(0, mirror_height - last_y2 - img_height + 1)}
            };
            //std::memcpy(last_box, box, 16 * sizeof(int));
            const int props[4] = {box[0][2] * box[0][3], box[1][2] * box[1][3], box[2][2] * box[2][3], box[3][2] * box[3][3]};
            const int total = props[0] + props[1] + props[2] + props[3];
            /*wxMessageBox("Boxes\nX left "+format_int(box[0][0])+", "+format_int(box[0][1])+" -> "+format_int(box[0][2] + box[0][0])+
            ", "+format_int(box[0][3] + box[0][1])+"\nX right "+format_int(box[1][0])+", "+format_int(box[1][1])+
            " -> "+format_int(box[1][2] + box[1][0])+", "+format_int(box[1][3] + box[1][1])+"\nY up "+format_int(box[2][0])+
            ", "+format_int(box[2][1])+" -> "+format_int(box[2][2] + box[2][0])+", "+format_int(box[2][3] + box[2][1])+
            "\nY down "+format_int(box[3][0])+", "+format_int(box[3][1])+" -> "+format_int(box[3][2] + box[3][0])+
            ", "+format_int(box[3][3] + box[3][1])+"\n% left "+format_int(props[0])+"\n% right "+format_int(props[1])+
            "\n% up "+format_int(props[2])+"\n% down "+format_int(props[3]));*/
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
            for(size_t i = 0; i < 4; area -= props[i], ++i)
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
    last_x1 = xpos;
    last_y1 = ypos;
    last_x2 = xpos + img_width;
    last_y2 = ypos + img_height;
    if(kehys->working) wxQueueEvent(kehys, new wxThreadEvent());
    return wxThread::ExitCode(0x2710 + fetches.size());
}

Noutaja::~Noutaja()
{
    kehys->fetcher = 0;
}

Lataaja::~Lataaja()
{
    kehys->pix_loader = 0;
}

void Peili::thread_done(wxThreadEvent &event)
{
    mirror->Refresh();
    /*else if(unique)
    {
        bar->SetStatusText("Duplicate check done for " + format_int(pix), 0);
        if(dupl_found)
        {
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
    if(flow)
    {
        size_t load_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - load_begin).count();
        timer_pix.Start(load_time < time_pix ? time_pix - load_time : 0);
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

void Peili::wait_threads()
{
    // Stop data handling threads. Do it outside of GUI thread?
    timer_queue.Stop();
    timer_dir.Stop();
    timer_pix.Stop();
    if(pix_loader || fetcher)
    {
        working = false;
        do
        {
            Sleep(200);
        }
        while(pix_loader || fetcher);
    }
}

void Peili::OnExit(wxCloseEvent &event)
{
    wait_threads();
    Destroy();
}
