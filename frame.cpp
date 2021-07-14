#include "frame.h"
#include "AppIcon.xpm"

const wxString Peili::APP_VER = "2017.10.31";
const wxString Peili::HOT_KEYS = "Shortcuts\nLMBx2 = Full screen\nMMB = Exit app\nRMB = Remove duplicates"
"\nA = Previous file\nD = Next file\nE = Next random file\nS = Pause show\nW = Continue show"
"\nSPACE = Show current file in folder\nB = Show status bar\nC = Count LMB clicks\nL = Change screenplay"
"\nM = Reload folders periodically\nR = Reload folders";
std::list<Nouto> fetches;
std::list<Nouto>::iterator fetch;
std::random_device rng;// std::mt19937
std::unique_ptr<wxZipEntry> entry;
int last_x1, last_y1, last_x2, last_y2, loading_pixs;
bool filter_by_time, filter_by_size, filter_by_name, unzipping, rip_zip, gg_zip, upscale, queuing, randomize = true;
int recent_depth = 14, buffer_size = 0x20, trailer_size = 0x30;
wxString zip_name, trash, entry_name, unpack;
wxBitmap unzipped;
wxPen big_red_pen(*wxRED, 2);
wxPen big_green_pen(*wxGREEN, 4);

Peili::Peili(const wxString &title, const wxArrayString &paths, const wxString &settings, const wxArrayString &names) :
    wxFrame(0, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxBG_STYLE_PAINT),
    dirs_pixs(paths), keywords(names), time_pix(3000), time_dir(144000)
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
    filter_by_name = keywords.size() != 0;

    panel = new wxPanel(this);
    sizer = new wxBoxSizer(wxHORIZONTAL);
    spreader = new wxBoxSizer(wxVERTICAL);
    mirror = new wxPanel(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE);
    bar = new wxStatusBar(panel);
    spreader->Add(mirror, 1, wxEXPAND);
    spreader->Add(bar, 0, wxEXPAND);
    sizer->Add(spreader, 1, wxEXPAND);
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
    mirror->Bind(wxEVT_KEY_DOWN, &Peili::keyboard, this);
    timer_pix.Bind(wxEVT_TIMER, &Peili::load_pix, this);
    timer_dir.Bind(wxEVT_TIMER, &Peili::load_dir, this);
    timer_queue.Bind(wxEVT_TIMER, &Peili::load_merged, this);
    timer_zip.Bind(wxEVT_TIMER, &Peili::load_zip, this);
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
        if(fetches.size())
        fetches.erase(std::next(fetch, 1), fetches.end());
        working = true;
    }
    //rng.seed(std::chrono::system_clock::now().time_since_epoch().count());
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
                else if(filter_by_name)
                for(const wxString &key: keywords)
                {
                    wxDir::GetAllFiles(dirs_pixs[i], &pixs, "*" + key + "*.jpg");
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

void Peili::load_merged(wxTimerEvent&)
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

void Peili::load_dir(wxTimerEvent&)
{
    timer_dir.Stop();
    load_pixs();
}

void Peili::load_pix(wxTimerEvent&)
{
    timer_pix.Stop();
    if(pixs.IsEmpty()) return;
    load_begin = std::chrono::system_clock::now();
    advance();
    load_image();
}

void Peili::load_zip(wxTimerEvent&)
{
    wxKeyEvent event(wxEVT_KEY_DOWN);
    event.m_keyCode = 'D';
    keyboard(event);
    timer_zip.Start(time_pix);
}

void Peili::draw_pixs(wxPaintEvent&)
{
    wxBufferedPaintDC dc(mirror);
    if(clean_mirror && !queuing)
    {
        dc.SetBackground(*wxBLACK_BRUSH);
        dc.Clear();
        unzipped = wxBitmap();
        clean_mirror = false;
    }
    if(unzipping)
    {
        if(unzipped.IsOk())
        {
            bool draw = true;
            if(minion)
            {
                int width, height;
                dc.GetSize(&width, &height);
                if(unzipped.GetWidth() < unzipped.GetHeight())
                {
                    if(width > height)
                    {
                        draw = false;
                        minion->Refresh();
                    }
                }
                else
                {
                    if(width < height)
                    {
                        draw = false;
                        minion->Refresh();
                    }
                }
            }
            if(draw)
            {
                dc.DrawBitmap(unzipped, 0, 0, true);
                dc.SetBackgroundMode(wxPENSTYLE_SOLID);
                dc.DrawText(zip_name, 2, 2);
                dc.DrawText(entry_name, 2, 20);
                queuing = false;
            }
        }
        if(rip_zip)
        {
            dc.SetPen(big_red_pen);
            dc.DrawLine(0, 0, 720, 720);
            dc.DrawLine(0, 720, 720, 0);
            rip_zip = false;
        }
        else if(gg_zip)
        {
            dc.SetPen(big_green_pen);
            dc.DrawLine(0, 360, 180, 540);
            dc.DrawLine(180, 540, 720, 0);
            gg_zip = false;
        }
    }
    else
    {
        bar->SetStatusText("Images shown: " + format_int(++drawn_cnt), 0);
        if(fetch != fetches.end() && fetch->file.IsOk())
        {
            dc.DrawBitmap(fetch->file, last_x1, last_y1, true);
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

void Peili::draw_side(wxPaintEvent&)
{
    wxBufferedPaintDC dc(minion);
    if(unzipping)
    {
        if(unzipped.IsOk())
        {
            dc.DrawBitmap(unzipped, 0, 0, true);
            dc.SetBackgroundMode(wxPENSTYLE_SOLID);
            dc.DrawText(zip_name, 2, 2);
            dc.DrawText(entry_name, 2, 20);
            queuing = false;
        }
    }
    else
    {
        bar->SetStatusText("Images shown: " + format_int(++drawn_cnt), 0);
        if(fetch != fetches.end() && fetch->file.IsOk())
        {
            dc.DrawBitmap(fetch->file, last_x1, last_y1, true);
        }
    }
}

void Peili::advance()
{
    wxCriticalSectionLocker lock(fetch_cs);
    // Advance if there is next item ready. TODO: Wait until it is ready?
    if(fetches.size())
    if(std::next(fetch, 1) != fetches.end())
    {
        // Skip first item as it will be popped out.
        std::advance(fetch, fetch == fetches.end() ? 2 : 1);
        int trail = std::distance(fetches.begin(), fetch);
        bar->SetStatusText("Trail: " + format_int(trail), 2);
        if(trail > trailer_size)
        {
            fetches.erase(fetches.begin(), std::prev(fetch, buffer_size));
        }
    }
}

void Peili::left_down(wxMouseEvent&)
{
    ++lmb_down;
    mirror->SetFocus();
}

void Peili::left_up(wxMouseEvent&)
{
    ++lmb_up;
}

void Peili::left_click(wxMouseEvent&)
{
    full_screen = !full_screen;
    ShowFullScreen(full_screen);
    if(minion)
    {
        minion->ShowFullScreen(full_screen);
    }
}

void Peili::middle_click(wxMouseEvent&)
{
    wxCloseEvent ce;
    OnExit(ce);
}

/*void Peili::right_click(wxMouseEvent&)
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
        case 'A':
        {
            if(unzipping)
            {
                if(queuing) return;
                unzip_image(--cur_entry);
            }
            else
            {
                stop_flow();
                if(std::prev(fetch, 1) != fetches.end()) --fetch;
                load_image();
            }
            return;
        }
        case 'D':
        {
            if(unzipping)
            {
                if(queuing) return;
                unzip_image(++cur_entry);
            }
            else
            {
                if(std::next(fetch, 1) != fetches.end()) ++fetch;
                load_image();
            }
            return;
        }
        case 'W':
        {
            if(unzipping)
            {
                if(queuing) return;
                if(zip_list)
                {
                    if(zip_list->GetCount())
                    {
                        wxCommandEvent select(wxEVT_LISTBOX);
                        select.SetInt(std::max(zip_list->GetSelection() - 1, 0));
                        zip_list->SetSelection(select.GetSelection());
                        zip_list->GetEventHandler()->ProcessEvent(select);
                    }
                }
                else
                {
                    cur_zip = (cur_zip - 1 + zips.GetCount()) % zips.GetCount();
                    unzip_image();
                }
            }
            else
            {
                flow = true;
                load_pixs();
            }
            return;
        }
        case 'S':
        {
            if(unzipping)
            {
                if(queuing) return;
                if(zip_list)
                {
                    if(zip_list->GetCount())
                    {
                        wxCommandEvent select(wxEVT_LISTBOX);
                        select.SetInt(std::min(zip_list->GetSelection() + 1, int(zip_list->GetCount()) - 1));
                        zip_list->SetSelection(select.GetSelection());
                        zip_list->GetEventHandler()->ProcessEvent(select);
                    }
                }
                else
                {
                    cur_zip = (cur_zip + 1) % zips.GetCount();
                    unzip_image();
                }
            }
            else
            {
                stop_flow();
            }
            return;
        }
        case 'E':
        {
            if(unzipping)
            {
                if(queuing) return;
                //unzip_image(-1);
                timer_zip.Start(time_pix);
            }
            else
            {
                timer_pix.Start(0);
            }
            return;
        }
        case 'P':
        {
            if(unzipping)
            {
                unzip_image(cur_entry);
            }
            else
            {
                load_image();
            }
            return;
        }
        case ' ':
        {
            if(unzipping)
            {
                wxExecute("Explorer /select," + zips[cur_zip]);
            }
            else
            {
                stop_flow();
                if(fetch != fetches.end())
                {
                    wxExecute("Explorer /select," + fetch->filename);
                }
            }
            return;
        }
        case 'B':
        {
            show_status_bar = !show_status_bar;
            bar->Show(show_status_bar);
            sizer->Layout();
            return;
        }
        case 'C':
        {
            cnt_clicks = !cnt_clicks;
            return;
        }
        case 'L':
        {
            shotgun = shotgun == Scene::ALL_OVER ? Scene::AVOID_LAST : Scene::ALL_OVER;
            return;
        }
        case 'M':
        {
            auto_dir_reload = !auto_dir_reload;
            if(auto_dir_reload) timer_dir.Start(time_pix);
            clean_mirror = true;
            return;
        }
        case 'F':
        {
            time_pix -= 100;
            bar->SetStatusText("Interval: " + format_int(time_pix), 4);
            return;
        }
        case 'K':
        {
            time_pix += 100;
            bar->SetStatusText("Interval: " + format_int(time_pix), 4);
            return;
        }
        case 'Z': // Toggle ZIP mode on/off.
        {
            unzipping = !unzipping;
            if(unzipping) // Set folder whose ZIP files are browsed.
            {
                wait_threads();
                wxDirDialog dialog(this, "Folder containing ZIPs");
                if(dialog.ShowModal() == wxID_OK)
                {
                    zips.Clear();
                    cur_zip = 0;
                    wxDir::GetAllFiles(dialog.GetPath(), &zips, "*.zip");
                    unzip_image();
                }
            }
            return;
        }
        case 'X': // Delete ZIP file.
        {
            remove_zip();
            next_entry();
            return;
        }
        case 'Q': // Move ZIP file to unpack folder.
        {
            if(!unpack)
            {
                wxDirDialog dialog(this, "Choose folder for unpack");
                if(dialog.ShowModal() == wxID_OK)
                {
                    unpack = dialog.GetPath() + "\\";
                    wxFileConfig settings("Kuvastin", "Tapsa");
                    settings.Write("Unpack", unpack);
                }
                else return;
            }
            gg_zip = true;
            if(wxFileName(zips[cur_zip]).FileExists())
            wxRenameFile(zips[cur_zip], unpack + zips[cur_zip].AfterLast('\\'), false);
            next_entry();
            return;
        }
        case 'T':
        {
            filter_by_time = !filter_by_time;
            break;
        }
        case 'R':
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
        case 'G': // Show only large files.
        {
            filter_by_size = !filter_by_size;
            break;
        }
        case 'N':
        {
            filter_by_name = !filter_by_name;
            break;
        }
        case 'I':
        {
            randomize = !randomize;
            break;
        }
        case 'Y':
        {
            buffer_size <<= 1;
            trailer_size <<= 1;
            if ( buffer_size > 0x80 )
            {
                buffer_size = 0x20;
            }
            if ( trailer_size > 0xC0 )
            {
                trailer_size = 0x30;
            }
            return;
        }
        case 'O':
        {
            wxTextEntryDialog dialog(this, "New keywords, separated by |");
            if(dialog.ShowModal() == wxID_OK)
            {
                keywords = wxStringTokenize(dialog.GetValue(), "|");
                filter_by_name = keywords.size() != 0;
                break;
            }
            else return;
        }
        case 'V':
        {
            upscale = !upscale;
            if(unzipping)
            {
                unzip_image(cur_entry);
                return;
            }
            load_image();
            break;
        }
        case 'U':
        {
            zip_list = new wxListBox(panel, wxID_ANY, wxDefaultPosition, wxSize(200, -1));
            sizer->Insert(0, zip_list, 0, wxEXPAND);
            wait_threads();

            wxFileConfig settings("Kuvastin", "Tapsa");
            settings.Read("Trash", &trash);
            settings.Read("Unpack", &unpack);
            wxFlexGridSizer *options_grid = new wxFlexGridSizer(2);
            wxStaticText *label1 = new wxStaticText(panel, wxID_ANY, " Path to zips ");
            wxDirPickerCtrl *path2zips = new wxDirPickerCtrl(panel, wxID_ANY, settings.Read("Path2zips", ""));
            wxStaticText *label3 = new wxStaticText(panel, wxID_ANY, " Path to unzips ");
            wxDirPickerCtrl *path2unzips = new wxDirPickerCtrl(panel, wxID_ANY, settings.Read("Path2unzips", ""));
            wxStaticText *label2 = new wxStaticText(panel, wxID_ANY, " Search for ");
            wxTextCtrl *zip_search = new wxTextCtrl(panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
            wxStaticText *label4 = new wxStaticText(panel, wxID_ANY, " 1st file in zip ");
            wxStaticText *first_file = new wxStaticText(panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxST_NO_AUTORESIZE);
            wxStaticText *label6 = new wxStaticText(panel, wxID_ANY, " 1st match in unzips ");
            wxStaticText *first_ufile = new wxStaticText(panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxST_NO_AUTORESIZE);
            wxStaticText *label5 = new wxStaticText(panel, wxID_ANY, " Common name until ");
            wxTextCtrl *name_clipper = new wxTextCtrl(panel, wxID_ANY, "_", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);

            path2zips->Bind(wxEVT_DIRPICKER_CHANGED, [](wxFileDirPickerEvent &event)
            {
                wxFileConfig settings("Kuvastin", "Tapsa");
                settings.Write("Path2zips", event.GetPath());
            });

            path2unzips->Bind(wxEVT_DIRPICKER_CHANGED, [](wxFileDirPickerEvent &event)
            {
                wxFileConfig settings("Kuvastin", "Tapsa");
                settings.Write("Path2unzips", event.GetPath());
            });

            zip_search->Bind(wxEVT_COMMAND_TEXT_ENTER, [=](wxCommandEvent&)
            {
                zips.Clear();
                cur_zip = 0;
                wxArrayString listed_names;
                wxDir::GetAllFiles(path2zips->GetPath(), &zips, "*" + zip_search->GetValue() + "*.zip");
                for(const wxString& name: zips)
                {
                    listed_names.Add(name.AfterLast('\\'));
                }
                zip_list->Set(listed_names);
                zip_list->SetBackgroundColour(*wxWHITE);
                unzip_image();
            });

            auto split_entry_name = [=]()
            {
                int axe_pos = entry_name.rfind(name_clipper->GetValue()) + 1;
                wxString common_part = entry_name.Left(axe_pos), uname;

                if(common_part.Len() > 1)
                {
                    wxArrayString pixs;
                    wxDir::GetAllFiles(path2unzips->GetPath(), &pixs, common_part + "*.jpg");
                    uname = pixs.GetCount() ? pixs[0].AfterLast('\\') : "";
                    unzip_image(0, uname);
                    zip_list->SetBackgroundColour(uname.Len() ? wxColour(100, 240, 100) : wxColour(240, 100, 100));
                }
                else zip_list->SetBackgroundColour(*wxWHITE);

                wxString remainder = entry_name.Mid(axe_pos);
                first_file->SetLabel(common_part + "|" + remainder);
                first_ufile->SetLabel(uname);
                zip_list->Refresh();
            };

            zip_list->Bind(wxEVT_LISTBOX, [=](wxCommandEvent &event)
            {
                cur_zip = event.GetSelection();
                // Get entry name
                unzip_image();
                split_entry_name();
            });

            name_clipper->Bind(wxEVT_COMMAND_TEXT_ENTER, [=](wxCommandEvent&)
            {
                split_entry_name();
            });

            options_grid->Add(label1);
            options_grid->Add(path2zips, 1, wxEXPAND);
            options_grid->Add(label3);
            options_grid->Add(path2unzips, 1, wxEXPAND);
            options_grid->Add(label2);
            options_grid->Add(zip_search, 1, wxEXPAND);
            options_grid->Add(label4);
            options_grid->Add(first_file, 1, wxEXPAND);
            options_grid->Add(label6);
            options_grid->Add(first_ufile, 1, wxEXPAND);
            options_grid->Add(label5);
            options_grid->Add(name_clipper);
            options_grid->AddGrowableCol(1, 1);
            spreader->Insert(0, options_grid, 0, wxEXPAND);
            sizer->Layout();
            unzipping = true;
            return;
        }
        case 'H':
        {
            if (!minion)
            {
                minion = new wxFrame(this, wxID_ANY, "Sivupeili", wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxBG_STYLE_PAINT);
                minion->SetSize(720, 900);
                minion->SetIcon(wxIcon(AppIcon_xpm));
                minion->Show(true);
                minion->Bind(wxEVT_PAINT, &Peili::draw_side, this);
            }
            else
            {
                minion->Destroy();
                delete minion;
                minion = nullptr;
            }
           return;
        }
        default: return;
    }
    timer_queue.Start(0);
}

void Peili::remove_zip()
{
    if(!trash)
    {
        wxDirDialog dialog(this, "Choose folder for trash");
        if(dialog.ShowModal() == wxID_OK)
        {
            trash = dialog.GetPath() + "\\";
            wxFileConfig settings("Kuvastin", "Tapsa");
            settings.Write("Trash", trash);
        }
        else return;
    }
    rip_zip = true;
    if(wxFileName(zips[cur_zip]).FileExists())
    wxRenameFile(zips[cur_zip], trash + zips[cur_zip].AfterLast('\\'), false);
}

void Peili::unzip_image(int random, wxString name)
{
    cur_entry = 0;
    entry_name = "";
    if(!(cur_zip < zips.GetCount() && wxFileName(zips[cur_zip]).FileExists()))
    {
        clean_mirror = true;
        mirror->Refresh();
        return;
    }
    bool remove = false;
    {
        wxFFileInputStream necessity(zips[cur_zip]);
        wxZipInputStream zip(necessity);
        int mirror_width, mirror_height, minion_width = 0, minion_height = 0;
        mirror->GetClientSize(&mirror_width, &mirror_height);
        if(minion)
        {
            minion->GetClientSize(&minion_width, &minion_height);
        }
        entry.reset(zip.GetNextEntry());
        if(name.Len())
        {
            while(entry && name != entry->GetName())
            {
                entry.reset(zip.GetNextEntry());
                ++cur_entry;
            }
        }
        else if(random < 0)
        {
            int dart = rng() % zip.GetTotalEntries();
            while(--dart > 0)
            {
                entry.reset(zip.GetNextEntry());
                ++cur_entry;
            }
        }
        else if(random > 0)
        {
            while(entry && size_t(random) > cur_entry)
            {
                entry.reset(zip.GetNextEntry());
                ++cur_entry;
            }
        }
        if(entry)
        {
            wxImage pic(zip, wxBITMAP_TYPE_JPEG);
            zip_name = zips[cur_zip].AfterLast('\\');
            entry_name = entry->GetName();
            int img_width = pic.GetWidth(), img_height = pic.GetHeight();
            // Determine which surface suits the photo better.
            if(img_width < img_height)
            {
                if(mirror_width > minion_width)
                {
                    mirror_width = minion_width;
                    mirror_height = minion_height;
                }
            }
            else
            {
                if(mirror_width < minion_width)
                {
                    mirror_width = minion_width;
                    mirror_height = minion_height;
                }
            }
            // Do the scaling.
            int leftover_width = mirror_width - img_width, leftover_height = mirror_height - img_height;
            if(img_width < 640 || img_height < 640)
            {
                remove = true;
            }
            if(upscale || leftover_width < 0 || leftover_height < 0)
            {
                float prop = leftover_width < leftover_height ? (float) mirror_width / img_width : (float) mirror_height / img_height;
                img_width *= prop;
                img_height *= prop;
                pic.Rescale(img_width, img_height, wxIMAGE_QUALITY_BICUBIC);
            }
            queuing = true;
            unzipped = wxBitmap(pic);
        }
        else clean_mirror = true;
    }
    if(allow_del && remove)
    {
        remove_zip();
    }
    next_entry();
}

void Peili::next_entry()
{
    mirror->Refresh();
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
    for(int i = 0; kehys->working && i < trailer_size; ++i)
    {
        kehys->folder_cs.Enter();
        size_t rng_beg = 0, rng_cnt = kehys->pixs.GetCount();
        if(kehys->equal_mix)
        {
            size_t ranges_cnt = kehys->path_ranges.size() >> 1, slot = rng() % ranges_cnt;
            rng_cnt = kehys->path_ranges[ranges_cnt + slot];
            rng_beg = kehys->path_ranges[slot];
        }
        if(0 == rng_cnt)
        {
            kehys->folder_cs.Leave();
            continue;
        }
        static size_t current;
        size_t next = randomize ? rng() : ++current;
        wxString picname(kehys->pixs[rng_beg + next % rng_cnt]);

        wxLogNull log;
        wxImage pic(picname, wxBITMAP_TYPE_JPEG);
        kehys->folder_cs.Leave();
        if(!pic.IsOk()) continue;

        int mirror_width, mirror_height, minion_width = 0, minion_height = 0;
        kehys->mirror->GetClientSize(&mirror_width, &mirror_height);
        if(kehys->minion)
        {
            kehys->minion->GetClientSize(&minion_width, &minion_height);
        }
        // Crop edges.
        {
            unsigned char val = pic.GetRed(0, 0), max_diff = 12;
            int width = pic.GetWidth(), height = pic.GetHeight(), density = 8;
            int crop_top = 0, crop_bottom = 0, crop_left = 0, crop_right = 0;
            for(int h = 0; h < height; ++h)
            for(int w = 0; w < width; w += density)
            {
                unsigned char red = pic.GetRed(w, h);
                if(std::abs(val - red) > max_diff)
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
                if(std::abs(val - red) > max_diff)
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
                if(std::abs(val - red) > max_diff)
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
                if(std::abs(val - red) > max_diff)
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
        int img_width = pic.GetWidth(), img_height = pic.GetHeight();
        if(kehys->allow_del && ((img_width < 960 && img_height < 960) || img_width < 640 || img_height < 640))
        {
            wxCriticalSectionLocker lock(kehys->folder_cs);
            wxRemoveFile(picname);
        }
        if(filter_by_size && img_width < mirror_width && img_height < mirror_height)
        {
            continue;
        }
        int leftover_width = mirror_width - img_width, leftover_height = mirror_height - img_height;
        if(upscale || leftover_width < 0 || leftover_height < 0)
        {
            float prop = std::fmin((float) mirror_width / img_width, (float) mirror_height / img_height);
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
    if(!kehys->fetcher && buffer < buffer_size)
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

void Peili::thread_done(wxThreadEvent&)
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

void Peili::OnExit(wxCloseEvent&)
{
    wait_threads();
    if(minion)
    {
        minion->Destroy();
    }
    Destroy();
}
