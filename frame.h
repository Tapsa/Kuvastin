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
    Peili(const wxString &title, wxString aP = wxEmptyString, bool allow_del = true);
    void OnExit(wxCloseEvent &event);
    void draw_pixs(wxPaintEvent &event);
    void clear_mirror(wxEraseEvent &event){}
    void load_pix(wxTimerEvent &event);
    void load_dir(wxTimerEvent &event);
    void left_click(wxMouseEvent &event);
    void middle_click(wxMouseEvent &event);
    void right_click(wxMouseEvent &event);
    void thread_done(wxThreadEvent &event);
    void keyboard(wxKeyEvent &event);

    void load_pixs();
    void load_image();

    static const wxString APP_VER;

    wxString arg_path, dir_pixs, pic_types, filename;
    wxArrayString pixs;
    wxBitmap pic;
    wxTimer timer_pix, timer_dir;
    unsigned int drawn_cnt, time_pix, time_dir;
    std::minstd_rand0 rng;
    bool full_screen, clean_mirror, unique, dupl_found, allow_del, inspect, browsing;
    std::chrono::time_point<std::chrono::system_clock> last_click, load_begin;
    int pix, picX, picY;
    std::list<wxString> shown_pixs;
    std::list<wxString>::iterator inspect_file;

protected:
    Lataaja *pix_loader;
    wxCriticalSection pix_loader_cs, dir_loader_cs;
    friend class Lataaja;

private:
    wxPanel *mirror;
    std::set<std::string> unique_pixs;
};

#endif
