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
    Peili(const wxString &title, wxString aP = wxEmptyString);
    void OnExit(wxCloseEvent &event);
    void draw_pixs(wxPaintEvent &event);
    void clear_mirror(wxEraseEvent &event){}
    void load_pix(wxTimerEvent &event);
    void left_click(wxMouseEvent &event);
    void middle_click(wxMouseEvent &event);

    void load_pixs();
    void load_image();

    static const wxString APP_VER;

    wxString arg_path, dir_pixs;
    wxArrayString pixs;
    wxBitmap pic;
    wxTimer timer_pix, timer_dir;
    unsigned int drawn_cnt, time_pix, time_dir;
    std::minstd_rand0 rng;
    bool full_screen, clean_mirror;
    std::chrono::time_point<std::chrono::system_clock> last_click;
    int picX, picY;

protected:
    Lataaja *pix_loader;
    wxCriticalSection pix_loader_cs;
    friend class Lataaja;

private:
    wxPanel *mirror;
};

#endif
