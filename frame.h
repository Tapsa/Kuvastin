#ifndef frame_h
#define frame_h

#include "common.h"

wxString format_float(float);
wxString format_int(int);

class Peili;

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
    Peili(const wxString&, const wxArrayString&, const wxString&, const wxArrayString&);

    void load_pixs();
    void load_image();
    inline void advance();
    void wait_threads();
    inline void stop_flow();
    void unzip_image(int = 0, wxString = "");
    void next_entry();
    void remove_zip();
    void position_image(const wxWindow *window, const wxBitmap *bitmap);

    static const wxString APP_VER, HOT_KEYS;

    enum class Scene
    {
        LEFT_TO_RIGHT,
        RIGHT_TO_LEFT,
        AVOID_LAST,
        ALL_OVER
    };

    wxArrayString pixs, dirs_pixs, keywords, zips;
    size_t drawn_cnt = 0, time_pix, time_dir, cur_zip = 0, cur_entry = 0;
    bool full_screen = false, clean_mirror = false, unique = false, dupl_found = false, allow_del = false, flow = true,
        equal_mix = false, cnt_clicks = false, auto_dir_reload = false, working = true, show_status_bar = false;
    std::chrono::time_point<std::chrono::system_clock> load_begin;
    unsigned lmb_down = 0, lmb_up = 0;
    Scene shotgun = Scene::AVOID_LAST;
    std::vector<size_t> path_ranges;

protected:
    Noutaja *fetcher = nullptr;
    Lataaja *pix_loader = nullptr;
    wxFrame *minion = nullptr;
    wxCriticalSection folder_cs, fetch_cs;
    friend class Noutaja;
    friend class Lataaja;

private:
    void OnExit(wxCloseEvent &event);
    void draw_pixs(wxPaintEvent &event);
    void draw_side(wxPaintEvent &event);
    void clear_mirror(wxEraseEvent&){}
    void load_pix(wxTimerEvent &event);
    void load_dir(wxTimerEvent &event);
    void load_merged(wxTimerEvent &event);
    void load_zip(wxTimerEvent &event);
    void left_click(wxMouseEvent &event);
    void left_down(wxMouseEvent &event);
    void left_up(wxMouseEvent &event);
    void middle_click(wxMouseEvent &event);
    //void right_click(wxMouseEvent &event);
    void thread_done(wxThreadEvent &event);
    void keyboard(wxKeyEvent &event);

    wxPanel *panel, *mirror;
    wxSizer *sizer, *spreader;
    wxStatusBar *bar;
    wxListBox *zip_list = 0;
    wxTimer timer_pix, timer_dir, timer_queue, timer_zip;
    std::set<std::string> unique_pixs;
};

#endif
