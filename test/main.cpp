/////////////////////////////////////////////////////////////////////////////
// Author:      PB
// Purpose:     Minimal application using wxSystemInformationFrame
//              (used to check that wxsysinfoframe builds)
// Copyright:   (c) 2019-2025 PB <pbfordev@gmail.com>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include <wx/wxprec.h>

#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif

#include "wxsysinfoframe.h"

class TestApp : public wxApp
{
public:
    bool OnInit() override
    {
        if ( !wxApp::OnInit() )
            return false;

        wxSystemInformationFrame* const frame = new wxSystemInformationFrame(nullptr);

        // check that the frame is usable
        if ( frame->GetValues().empty() )
        {
            wxLogError("wxSystemInformationFrame returned no values.");
            frame->Destroy();
            return false;
        }

        frame->Show();
        return true;
    }
};

wxIMPLEMENT_APP(TestApp);
