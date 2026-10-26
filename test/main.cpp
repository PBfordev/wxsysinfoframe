/////////////////////////////////////////////////////////////////////////////
// Author:      PB
// Purpose:     Minimal application using wxSystemInformationFrame
//              (used to check that wxsysinfoframe builds and works)
// Copyright:   (c) 2019-2026 PB <pbfordev@gmail.com>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include <wx/wxprec.h>

#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif

#include <wx/cmdline.h>
#include <wx/ffile.h>
#include <wx/timer.h>

#include "wxsysinfoframe.h"

namespace {

// Collects the log messages, in the dump mode there is nobody to close
// message boxes shown by the default log target.
class CollectingLog : public wxLog
{
public:
    const wxArrayString& GetMessages() const { return m_messages; }
    size_t GetErrorCount() const { return m_errorCount; }

protected:
    void DoLogRecord(wxLogLevel level, const wxString& msg, const wxLogRecordInfo&) override
    {
        const char* levelName = "Message";

        switch ( level )
        {
            case wxLOG_FatalError: levelName = "Fatal error"; break;
            case wxLOG_Error:      levelName = "Error"; break;
            case wxLOG_Warning:    levelName = "Warning"; break;
            default: break;
        }

        if ( level <= wxLOG_Error )
            ++m_errorCount;

        m_messages.push_back(wxString::Format("%s: %s", levelName, msg));
    }

private:
    wxArrayString m_messages;
    size_t        m_errorCount{0};
};

} // anonymous namespace

class TestApp : public wxApp
{
public:
    TestApp()
    {
        m_timer.SetOwner(this);
        Bind(wxEVT_TIMER, &TestApp::OnTimer, this);
    }

    void OnInitCmdLine(wxCmdLineParser& parser) override
    {
        wxApp::OnInitCmdLine(parser);
        parser.AddOption("d", "dump",
            "write the system information to the given file and exit, "
            "the exit code is non-zero if there was a problem",
            wxCMD_LINE_VAL_STRING);
    }

    bool OnCmdLineParsed(wxCmdLineParser& parser) override
    {
        if ( !wxApp::OnCmdLineParsed(parser) )
            return false;

        m_dump = parser.Found("d", &m_dumpFileName);
        return true;
    }

    bool OnInit() override
    {
        if ( !wxApp::OnInit() )
            return false;

        if ( m_dump )
        {
            // takes over from the default log target
            m_log = new CollectingLog;
            delete wxLog::SetActiveTarget(m_log);
        }

        m_frame = new wxSystemInformationFrame(nullptr);

        if ( m_dump )
        {
            // the full host name is obtained in a background thread,
            // so the values are collected a bit later
            m_timer.Start(250);
        }
        else if ( m_frame->GetValues().empty() )
        {
            wxLogError("wxSystemInformationFrame returned no values.");
            m_frame->Destroy();
            return false;
        }

        m_frame->Show();
        return true;
    }

    int OnRun() override
    {
        const int exitCode = wxApp::OnRun();

        return m_failed ? 1 : exitCode;
    }

private:
    wxSystemInformationFrame* m_frame{nullptr};
    CollectingLog*            m_log{nullptr};
    wxTimer                   m_timer;
    bool                      m_dump{false};
    wxString                  m_dumpFileName;
    bool                      m_failed{false};
    int                       m_timerTicks{0};

    void OnTimer(wxTimerEvent&)
    {
        const int maxTicks = 60; // 15 seconds in total
        const wxArrayString values = m_frame->GetValues();
        bool stillEvaluating = false;

        for ( const auto& value : values )
        {
            if ( value.Contains("<Evaluating...>") )
            {
                stillEvaluating = true;
                break;
            }
        }

        if ( stillEvaluating && ++m_timerTicks < maxTicks )
            return;

        m_timer.Stop();
        Dump(values);
        m_frame->Close();
    }

    void Dump(const wxArrayString& values)
    {
        wxString text;

        for ( const auto& value : values )
            text += value + "\n";

        if ( !m_log->GetMessages().empty() )
        {
            text += "\nLog messages\n----------------------------\n";
            for ( const auto& message : m_log->GetMessages() )
                text += message + "\n";
        }

        wxFFile file(m_dumpFileName, "wb");

        if ( !file.IsOpened() || !file.Write(text, wxConvUTF8) )
        {
            m_failed = true;
            return;
        }

        // no values or an error message is a failure
        m_failed = values.empty() || m_log->GetErrorCount() > 0;
    }
};

wxIMPLEMENT_APP(TestApp);
