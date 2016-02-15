#ifndef frame_h
#define frame_h

#include "common.h"

wxString format_float(float);
wxString format_int(int);

class Peili;

class Lataaja: public wxThread
{
public:
    Lataaja(Peili *kehys):
    wxThread()
    {
        this->kehys = kehys;
    }
    ~Lataaja();

protected:
    virtual ExitCode Entry();
    Peili *kehys;
};

class Peili: public wxFrame
{
public:
    Peili(const wxString&, const wxArrayString&, const wxString&);
    void OnExit(wxCloseEvent &event);
    void draw_pixs(wxPaintEvent &event);
    void clear_mirror(wxEraseEvent &event){}
    void load_pix(wxTimerEvent &event);
    void load_dir(wxTimerEvent &event);
    void left_click(wxMouseEvent &event);
    void left_down(wxMouseEvent &event);
    void left_up(wxMouseEvent &event);
    void middle_click(wxMouseEvent &event);
    void right_click(wxMouseEvent &event);
    void thread_done(wxThreadEvent &event);
    void keyboard(wxKeyEvent &event);

    void load_pixs();
    void load_image();

    static const wxString APP_VER, HOT_KEYS;

    wxString pic_types = "*.jpg", filename;
    wxArrayString pixs, dirs_pixs;
    wxBitmap pic;
    wxTimer timer_pix, timer_dir;
    size_t drawn_cnt = 0, time_pix, time_dir;
    std::minstd_rand0 rng;
    bool full_screen = false, clean_mirror = false, unique = false, dupl_found = false, allow_del = false, inspect = false,
        browsing = false, equal_mix = false, cnt_clicks = false, auto_dir_reload = false, exit = false, show_status_bar = false;
    std::chrono::time_point<std::chrono::system_clock> load_begin;
    int pix, picX, picY, lmb_down = 0, lmb_up = 0;
    std::list<wxString> shown_pixs;
    std::list<wxString>::iterator inspect_file;
    std::vector<size_t> path_ranges;

protected:
    Lataaja *pix_loader = 0;
    wxCriticalSection pix_loader_cs, dir_loader_cs;
    friend class Lataaja;

private:
    wxSizer *sizer;
    wxPanel *mirror;
    wxStatusBar *bar;
    std::set<std::string> unique_pixs;
};

#endif
