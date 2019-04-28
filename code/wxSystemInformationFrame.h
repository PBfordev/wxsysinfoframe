/////////////////////////////////////////////////////////////////////////////
// Author:      PB
// Purpose:     wxSystemInformationFrame definition
// Copyright:   (c) 2019 PB <pbfordev@gmail.com>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef WX_SYSTEM_INFORMATION_FRAME_H_DEFINED
#define WX_SYSTEM_INFORMATION_FRAME_H_DEFINED


#define WX_SYSTEM_INFORMATION_FRAME_MAJOR_VERSION  1
#define WX_SYSTEM_INFORMATION_FRAME_MINOR_VERSION  0
#define WX_SYSTEM_INFORMATION_FRAME_VERSION_STRING "1.0"

#include <wx/wx.h>

class WXDLLIMPEXP_FWD_CORE wxNotebook; // to avoid including <wx/notebook.h> from a header file

class wxSystemInformationFrame : public wxFrame
{
public:
    enum CreateFlags
    {
        // whether the values automatically refresh in response to
        // WM_SETTINGCHANGE, WM_DPICHANGED, wxEVT_SYS_COLOUR_CHANGED, and wxEVT_DISPLAY_CHANGED
        AutoRefresh = 1,

        // whether to display these pages with values, at least one of these
        // must be specified when creating wxSystemInformationFrame
        ViewSystemColours        = 1 << 1,
        ViewSystemFonts          = 1 << 2,
        ViewSystemMetrics        = 1 << 3,
        ViewDisplays             = 1 << 4,
        ViewStandardPaths        = 1 << 5,
        ViewSystemOptions        = 1 << 6,
        ViewEnvironmentVariables = 1 << 7,
        ViewMiscellaneous        = 1 << 8,
        ViewPreprocessorDefines  = 1 << 9,
    };

    static const long DefaultCreateFlags = AutoRefresh
                                           | ViewSystemColours | ViewSystemFonts | ViewSystemMetrics
                                           | ViewDisplays | ViewStandardPaths | ViewSystemOptions
                                           | ViewEnvironmentVariables | ViewMiscellaneous
                                           | ViewPreprocessorDefines;


    wxSystemInformationFrame(wxWindow *parent, wxWindowID id, const wxString &title,
                             const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
                             long frameStyle = wxDEFAULT_FRAME_STYLE,
                             long createFlags = DefaultCreateFlags);

    // this simple constructor creates the frame on a wxDefaultPosition,
    // with "wxSystemInformationFrame" as the title
    // and wxDEFAULT_FRAME_STYLE frameStyle
    wxSystemInformationFrame(wxWindow* parent, const wxSize& size = wxSize(1024, 800),
                             long createFlags = DefaultCreateFlags);

    bool Create(wxWindow *parent, wxWindowID id, const wxString &title,
                const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
                long frameStyle = wxDEFAULT_FRAME_STYLE,
                long createFlags = DefaultCreateFlags);

    void RefreshValues() { UpdateValues(); }

    // Returns the values for the visible views as the name and value pair separated
    // by the separator except for displays where there can be more than one display and
    // therefore value for each parameter.
    wxArrayString GetValues(const wxString& separator = "\t") const;

#ifdef __WXMSW__ // for WM_SETTINGCHANGE and WM_DPICHANGED
    WXLRESULT MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam) override;
#endif // #ifdef __WXMSW__

private:
    bool m_autoRefresh{true};

    wxNotebook* m_pages{nullptr};
    wxTextCtrl* m_logCtrl{nullptr};

    wxTimer m_valuesUpdateTimer;

    void LogInformation(const wxString& information);

    void TriggerValuesUpdate();
    void UpdateValues();

    void OnRefresh(wxCommandEvent&);
    void OnShowDetailedInformation(wxCommandEvent&);
    void OnShowwxInfoMessageBox(wxCommandEvent&);
    void OnSave(wxCommandEvent&);
    void OnUpdateUI(wxUpdateUIEvent& event);
    void OnUpdateValuesTimer(wxTimerEvent&);
    void OnSysColourChanged(wxSysColourChangedEvent& event);
    void OnDisplayChanged(wxDisplayChangedEvent& event);
};

#endif //ifndef WX_SYSTEM_INFORMATION_FRAME_H_DEFINED
