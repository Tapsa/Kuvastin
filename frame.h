#ifndef frame_h
#define frame_h

#include "common.h"

wxString format_float(float);
wxString format_int(int);

class Peili;

class Nouto
{
public:
    Nouto(const wxImage &f, const wxString &n, int v1, int v2, int v3, int v4, int v5, int v6):
    file(f), filename(n), mirror_width(v1), mirror_height(v2), img_width(v3), img_height(v4), leftover_width(v5), leftover_height(v6){}
    const wxImage file;
    const wxString filename;
    const int mirror_width, mirror_height, img_width, img_height, leftover_width, leftover_height;
};

class Noutaja: public wxThread
{
public:
    Noutaja(Peili *kehys):
    wxThread()
    {
        this->kehys = kehys;
    }
    ~Noutaja();

protected:
    virtual ExitCode Entry();
    Peili *kehys;
};

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

    void load_pixs();
    void load_image();
    void advance();

    static const wxString APP_VER, HOT_KEYS;

    enum Scene
    {
        LEFT_TO_RIGHT,
        RIGHT_TO_LEFT,
        AVOID_LAST,
        ALL_OVER
    };

    wxArrayString pixs, dirs_pixs;
    size_t drawn_cnt = 0, time_pix, time_dir;
    bool full_screen = false, clean_mirror = false, unique = false, dupl_found = false, allow_del = false, flow = true,
        equal_mix = false, cnt_clicks = false, auto_dir_reload = false, working = true, show_status_bar = false;
    std::chrono::time_point<std::chrono::system_clock> load_begin;
    unsigned pix, lmb_down = 0, lmb_up = 0, shotgun = AVOID_LAST;
    std::vector<size_t> path_ranges;

protected:
    Noutaja *fetcher = 0;
    Lataaja *pix_loader = 0;
    wxCriticalSection folder_cs, fetch_cs;
    friend class Noutaja;
    friend class Lataaja;

private:
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

    wxSizer *sizer;
    wxPanel *mirror;
    wxStatusBar *bar;
    wxTimer timer_pix, timer_dir;
    std::set<std::string> unique_pixs;
};

#endif
