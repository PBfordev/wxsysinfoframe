/////////////////////////////////////////////////////////////////////////////
// Author:      PB
// Purpose:     Implementation of wxSystemInformationFrame and its helpers
// Copyright:   (c) 2019-2026 PB <pbfordev@gmail.com>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include <wx/wxprec.h>

#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif

#if !wxCHECK_VERSION(3, 0, 0)
    #error wxSystemInformationFrame requires wxWidgets version 3 or higher
#endif

// Several of the headers below are included only so that the wxHAS_* and
// wxUSE_* macros reported by the Preprocessor Defines page are visible.
#include <wx/animate.h>
#include <wx/apptrait.h>
#include <wx/colordlg.h>
#include <wx/combo.h>
#include <wx/display.h>
#include <wx/dynlib.h>
#include <wx/filename.h>
#include <wx/fontdlg.h>
#include <wx/intl.h>
#include <wx/ipc.h>
#include <wx/listctrl.h>
#ifdef __WXMSW__
    #include <wx/msw/private.h>
#endif
#include <wx/nativewin.h>
#include <wx/notebook.h>
#include <wx/power.h>
#include <wx/settings.h>
#include <wx/stdpaths.h>
#if wxUSE_SYSTEM_OPTIONS
    #include <wx/sysopt.h>
#endif
#include <wx/textfile.h>
#include <wx/tglbtn.h>
#include <wx/thread.h>
#include <wx/timer.h>
#if wxCHECK_VERSION(3, 1, 6)
    #include <wx/uilocale.h>
#endif
#include <wx/utils.h>
#include <wx/wupdlock.h>

#ifdef __WXMSW__
    #include <cwchar>

    #include <uxtheme.h>
    #include <winuser.h>
#endif
#ifdef __WXGTK__
    #include <gtk/gtk.h>
#endif

// pull in the standard library version macros (_LIBCPP_VERSION etc.)
#if defined(__has_include)
    #if __has_include(<version>)
        #include <version>
    #else
        #include <ciso646>
    #endif
#else
    #include <ciso646>
#endif


#include <map>
#include <memory>
#include <set>
#include <vector>

#include "wxsysinfoframe.h"

namespace { // anonymous namespace for helper classes and functions

wxString wxRectTowxString(const wxRect& r)
{
    return wxString::Format("%d, %d; %d, %d",
        r.GetLeft(), r.GetTop(), r.GetRight(), r.GetBottom());
}

wxString wxSizeTowxString(const wxSize& s)
{
    return wxString::Format("%d x %d", s.x, s.y);
}


/*************************************************

    SysInfoListView

*************************************************/

class SysInfoListView : public wxListView
{
public:
    SysInfoListView(wxWindow* parent);

    void UpdateValues();
    void ShowDetailedInformation() const;

    virtual bool CanShowDetailedInformation() const { return false; }

    virtual wxArrayString GetValues(const wxString& separator = "\t") const = 0;
protected:
    std::map<long,int> m_columnWidths;

    long AppendItemWithData(const wxString& label, long data);

    virtual void DoUpdateValues() = 0;
    virtual void DoShowDetailedInformation(long WXUNUSED(listItemIndex)) const {}

    wxArrayString GetNameAndValueValues(int nameColumnIndex, int valueColumnIndex, const wxString& separator) const;

    void AutoSizeColumns();

    void OnColumnEndDrag(wxListEvent& event);
    void OnItemActivated(wxListEvent& event);
};

SysInfoListView::SysInfoListView(wxWindow* parent)
    : wxListView(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL)
{
    Bind(wxEVT_LIST_COL_END_DRAG, &SysInfoListView::OnColumnEndDrag, this);
    Bind(wxEVT_LIST_ITEM_ACTIVATED, &SysInfoListView::OnItemActivated, this);
}

void SysInfoListView::UpdateValues()
{
    wxWindowUpdateLocker updateLocker(this);

    // some views delete and recreate all items, remember the selection
    // so that it is not lost on each refresh
    const long previousSelection = GetFirstSelected();

    DoUpdateValues();
    AutoSizeColumns();

    const long itemCount = GetItemCount();

    if ( itemCount > 0 && GetFirstSelected() == -1 )
    {
        const long toSelect = (previousSelection != -1 && previousSelection < itemCount)
                              ? previousSelection : 0;

        Select(toSelect);
        Focus(toSelect);
        EnsureVisible(toSelect);
    }
}

void SysInfoListView::ShowDetailedInformation() const
{
    const long itemIndex = GetFirstSelected();

    if ( itemIndex != -1 )
        DoShowDetailedInformation(itemIndex);
}

long SysInfoListView::AppendItemWithData(const wxString& label, long data)
{
    const long itemIndex = InsertItem(GetItemCount(), label);

    if ( itemIndex == -1 )
        wxLogError(_("Could not insert item with label '%s'"), label);
    else
        SetItemData(itemIndex, data);

    return itemIndex;
}

wxArrayString SysInfoListView::GetNameAndValueValues(int nameColumnIndex, int valueColumnIndex, const wxString& separator) const
{
    const int itemCount = GetItemCount();

    wxArrayString values;

    values.reserve(itemCount + 1);

    // column headings
    values.push_back(wxString::Format(wxS("%s%s%s"), _("Name"), separator, _("Value")));

    // dump values
    for ( int i = 0; i < itemCount; ++i )
    {
        values.push_back(wxString::Format(wxS("%s%s%s"),
            GetItemText(i, nameColumnIndex),
            separator,
            GetItemText(i, valueColumnIndex)));
    }

    return values;
}

void SysInfoListView::AutoSizeColumns()
{
    const int columnCount = GetColumnCount();

    for ( int i = 0; i < columnCount; ++i )
    {
        const auto it = m_columnWidths.find(i);

        if ( it != m_columnWidths.end() )
            SetColumnWidth(i, it->second);
        else
            SetColumnWidth(i, wxLIST_AUTOSIZE);
    }
}

void SysInfoListView::OnColumnEndDrag(wxListEvent& event)
{
    event.Skip();
    m_columnWidths[event.GetColumn()] = GetColumnWidth(event.GetColumn());
}

void SysInfoListView::OnItemActivated(wxListEvent& event)
{
    DoShowDetailedInformation(event.GetIndex());
}


/*************************************************

    SystemSettingView

*************************************************/

class SystemSettingView : public SysInfoListView
{
public:
    SystemSettingView(wxWindow* parent);

    wxArrayString GetValues(const wxString& separator) const override
    {
        return GetNameAndValueValues(Column_Name, Column_Value, separator);
    }
protected:
    enum
    {
        Column_Name = 0,
        Column_Value,
        Column_Description
    };
};

SystemSettingView::SystemSettingView(wxWindow* parent)
    : SysInfoListView(parent)
{
    InsertColumn(Column_Name, _("Name"));
    InsertColumn(Column_Value, _("Value"));
    InsertColumn(Column_Description, _("Description"));
}


/*************************************************

    SystemColourView

*************************************************/

class SystemColourView : public SystemSettingView
{
public:
    SystemColourView(wxWindow* parent);

    bool CanShowDetailedInformation() const override { return GetFirstSelected() != -1; }

    wxColour GetColourBitmapOutlineColour() const { return m_outlineColour; }

    static wxColour GetDefaultColourBitmapOutlineColour();
protected:
    void DoUpdateValues() override;
    void DoShowDetailedInformation(long listItemIndex) const override;
private:
    wxColour m_outlineColour;
    std::set<wxSystemColour> m_deprecatedColourList;

    wxBitmap CreateColourBitmap(const wxColour& colour, const wxSize& size);

    void CreateDeprecatedColourList();
    bool IsDeprecatedSystemColour(wxSystemColour index) const;
};

struct ColourInfo
{
    wxSystemColour index;
    const char*    name;
    const char*    description;
};

ColourInfo const s_colourInfoArray[] =
{
    { wxSYS_COLOUR_SCROLLBAR, "wxSYS_COLOUR_SCROLLBAR", "The scrollbar grey area." },
    { wxSYS_COLOUR_DESKTOP, "wxSYS_COLOUR_DESKTOP", "The desktop colour." },
    { wxSYS_COLOUR_ACTIVECAPTION, "wxSYS_COLOUR_ACTIVECAPTION", "Active window caption colour." },
    { wxSYS_COLOUR_INACTIVECAPTION, "wxSYS_COLOUR_INACTIVECAPTION", "Inactive window caption colour." },
    { wxSYS_COLOUR_MENU, "wxSYS_COLOUR_MENU", "Menu background colour." },
    { wxSYS_COLOUR_WINDOW, "wxSYS_COLOUR_WINDOW", "Window background colour." },
    { wxSYS_COLOUR_WINDOWFRAME, "wxSYS_COLOUR_WINDOWFRAME", "Window frame colour." },
    { wxSYS_COLOUR_MENUTEXT, "wxSYS_COLOUR_MENUTEXT", "Colour of the text used in the menus." },
    { wxSYS_COLOUR_WINDOWTEXT, "wxSYS_COLOUR_WINDOWTEXT", "Colour of the text used in generic windows." },
    { wxSYS_COLOUR_CAPTIONTEXT, "wxSYS_COLOUR_CAPTIONTEXT", "Colour of the text used in captions, size boxes and scrollbar arrow boxes." },
    { wxSYS_COLOUR_ACTIVEBORDER, "wxSYS_COLOUR_ACTIVEBORDER", "Active window border colour." },
    { wxSYS_COLOUR_INACTIVEBORDER, "wxSYS_COLOUR_INACTIVEBORDER", "Inactive window border colour." },
    { wxSYS_COLOUR_APPWORKSPACE, "wxSYS_COLOUR_APPWORKSPACE", "Background colour for MDI applications." },
    { wxSYS_COLOUR_HIGHLIGHT, "wxSYS_COLOUR_HIGHLIGHT", "Colour of item(s) selected in a control." },
    { wxSYS_COLOUR_HIGHLIGHTTEXT, "wxSYS_COLOUR_HIGHLIGHTTEXT", "Colour of the text of item(s) selected in a control." },
    { wxSYS_COLOUR_BTNFACE, "wxSYS_COLOUR_BTNFACE", "Face shading colour on push buttons." },
    { wxSYS_COLOUR_BTNSHADOW, "wxSYS_COLOUR_BTNSHADOW", "Edge shading colour on push buttons." },
    { wxSYS_COLOUR_GRAYTEXT, "wxSYS_COLOUR_GRAYTEXT", "Colour of greyed (disabled) text." },
    { wxSYS_COLOUR_BTNTEXT, "wxSYS_COLOUR_BTNTEXT", "Colour of the text on push buttons." },
    { wxSYS_COLOUR_INACTIVECAPTIONTEXT, "wxSYS_COLOUR_INACTIVECAPTIONTEXT", "Colour of the text in inactive captions." },
    { wxSYS_COLOUR_BTNHIGHLIGHT, "wxSYS_COLOUR_BTNHIGHLIGHT", "Highlight colour for buttons." },
    { wxSYS_COLOUR_3DDKSHADOW, "wxSYS_COLOUR_3DDKSHADOW", "Dark shadow colour for three-dimensional display elements." },
    { wxSYS_COLOUR_3DLIGHT, "wxSYS_COLOUR_3DLIGHT", "Light colour for three-dimensional display elements." },
    { wxSYS_COLOUR_INFOTEXT, "wxSYS_COLOUR_INFOTEXT", "Text colour for tooltip controls." },
    { wxSYS_COLOUR_INFOBK, "wxSYS_COLOUR_INFOBK", "Background colour for tooltip controls." },
    { wxSYS_COLOUR_LISTBOX, "wxSYS_COLOUR_LISTBOX", "Background colour for list-like controls." },
    { wxSYS_COLOUR_HOTLIGHT, "wxSYS_COLOUR_HOTLIGHT", "Colour for a hyperlink or hot-tracked item." },
    { wxSYS_COLOUR_GRADIENTACTIVECAPTION, "wxSYS_COLOUR_GRADIENTACTIVECAPTION", "Right side colour in the color gradient of an active window's title bar." },
    { wxSYS_COLOUR_GRADIENTINACTIVECAPTION, "wxSYS_COLOUR_GRADIENTINACTIVECAPTION", "Right side colour in the color gradient of an inactive window's title bar." },
    { wxSYS_COLOUR_MENUHILIGHT, "wxSYS_COLOUR_MENUHILIGHT", "The colour used to highlight menu items when the menu appears as a flat menu." },
    { wxSYS_COLOUR_MENUBAR, "wxSYS_COLOUR_MENUBAR", "The background colour for the menu bar when menus appear as flat menus." },
    { wxSYS_COLOUR_LISTBOXTEXT, "wxSYS_COLOUR_LISTBOXTEXT", "Text colour for list-like controls." },
    { wxSYS_COLOUR_LISTBOXHIGHLIGHTTEXT, "wxSYS_COLOUR_LISTBOXHIGHLIGHTTEXT", "Text colour for the unfocused selection of list-like controls." },
    { wxSYS_COLOUR_BACKGROUND, "wxSYS_COLOUR_BACKGROUND", "Synonym for wxSYS_COLOUR_DESKTOP." },
    { wxSYS_COLOUR_3DFACE, "wxSYS_COLOUR_3DFACE", "Synonym for wxSYS_COLOUR_BTNFACE." },
    { wxSYS_COLOUR_3DSHADOW, "wxSYS_COLOUR_3DSHADOW", "Synonym for wxSYS_COLOUR_BTNSHADOW." },
    { wxSYS_COLOUR_BTNHILIGHT, "wxSYS_COLOUR_BTNHILIGHT", "Synonym for wxSYS_COLOUR_BTNHIGHLIGHT." },
    { wxSYS_COLOUR_3DHIGHLIGHT, "wxSYS_COLOUR_3DHIGHLIGHT", "Synonym for wxSYS_COLOUR_BTNHIGHLIGHT." },
    { wxSYS_COLOUR_3DHILIGHT, "wxSYS_COLOUR_3DHILIGHT", "Synonym for wxSYS_COLOUR_BTNHIGHLIGHT." },
    { wxSYS_COLOUR_FRAMEBK, "wxSYS_COLOUR_FRAMEBK", "Synonym for wxSYS_COLOUR_BTNFACE." },
};

SystemColourView::SystemColourView(wxWindow* parent)
    : SystemSettingView(parent)
{
    CreateDeprecatedColourList();

    m_outlineColour = GetDefaultColourBitmapOutlineColour();

    for ( size_t i = 0; i < WXSIZEOF(s_colourInfoArray); ++i )
    {
         wxString colourDescription = s_colourInfoArray[i].description;

         if ( IsDeprecatedSystemColour(s_colourInfoArray[i].index) )
             colourDescription += _(" (Deprecated on Windows 10 and newer.)");

         const long itemIndex = AppendItemWithData(s_colourInfoArray[i].name, (long)i);

         if ( itemIndex != -1 )
             SetItem(itemIndex, Column_Description, colourDescription);
    }

    UpdateValues();
}

wxColour SystemColourView::GetDefaultColourBitmapOutlineColour()
{
    const wxColour defaultOutlineColour(202, 31, 123); // magenta

    return defaultOutlineColour;
}

void SystemColourView::DoUpdateValues()
{
    wxSize size;

    size.SetWidth(wxSystemSettings::GetMetric(wxSYS_SMALLICON_X, this));
    size.SetHeight(wxSystemSettings::GetMetric(wxSYS_SMALLICON_Y, this));

    // work around wxSystemSettings::GetMetric(wxSYS_SMALLICON_{X|Y}) value
    // being unavailable
    if ( !size.IsFullySpecified() )
    {
        size.Set(16, 16);
#if wxCHECK_VERSION(3, 1, 0)
        size = FromDIP(size);
#endif
    }

    // the list control takes the ownership of the image list
    // and deletes the previous one
    wxImageList* const imageList = new wxImageList(size.GetWidth(), size.GetHeight(), false);

    AssignImageList(imageList, wxIMAGE_LIST_SMALL);

    const int itemCount = GetItemCount();

    for ( int i = 0; i < itemCount; ++i )
    {
         const wxColour colour = wxSystemSettings::GetColour(s_colourInfoArray[GetItemData(i)].index);
         const int imageIndex = imageList->Add(CreateColourBitmap(colour.IsOk() ? colour : GetColourBitmapOutlineColour(), size));
         wxString colourValue = _("<Invalid>");

         if ( colour.IsOk() )
         {
             colourValue = colour.GetAsString(wxC2S_CSS_SYNTAX);

             if ( !colour.IsSolid() )
                 colourValue += _(", not solid");
         }

         SetItem(i, Column_Value, colourValue, imageIndex);
    }
}

void SystemColourView::DoShowDetailedInformation(long listItemIndex) const
{
    const size_t infoArrayIndex = GetItemData(listItemIndex);
    const wxString valueName = s_colourInfoArray[infoArrayIndex].name;
    const wxColour colour = wxSystemSettings::GetColour(s_colourInfoArray[infoArrayIndex].index);

    if ( !colour.IsOk() )
    {
        wxLogError(_("Invalid colour for \"%s\"."), valueName);
        return;
    }

    wxColourData colourData;

    colourData.SetCustomColour(0, colour);

    wxGetColourFromUser(wxGetTopLevelParent(const_cast<SystemColourView*>(this)), colour, wxString::Format(_("Viewing %s"), valueName), &colourData);
}

wxBitmap SystemColourView::CreateColourBitmap(const wxColour& colour, const wxSize& size)
{
    wxBitmap bitmap(size);
    wxMemoryDC memoryDC(bitmap);
    wxBrush brush(colour);
    wxPen pen(m_outlineColour, FromDIP(1));
    wxDCPenChanger penChanger(memoryDC, pen);
    wxDCBrushChanger brushChanger(memoryDC, brush);

    memoryDC.DrawRectangle(wxRect(size));
    memoryDC.SelectObject(wxNullBitmap);

    return bitmap;
}

void SystemColourView::CreateDeprecatedColourList()
{
#ifdef __WXMSW__
// most system colors are deprecated in Windows 10 and newer
// see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getsyscolor

    int verMajor = 0;

    wxGetOsVersion(&verMajor);

    if ( verMajor < 10 )
        return;

    m_deprecatedColourList.insert(wxSYS_COLOUR_3DDKSHADOW);
    m_deprecatedColourList.insert(wxSYS_COLOUR_3DHIGHLIGHT);
    m_deprecatedColourList.insert(wxSYS_COLOUR_3DHILIGHT);
    m_deprecatedColourList.insert(wxSYS_COLOUR_3DLIGHT);
    m_deprecatedColourList.insert(wxSYS_COLOUR_3DSHADOW);
    m_deprecatedColourList.insert(wxSYS_COLOUR_ACTIVEBORDER);
    m_deprecatedColourList.insert(wxSYS_COLOUR_ACTIVECAPTION);
    m_deprecatedColourList.insert(wxSYS_COLOUR_APPWORKSPACE);
    m_deprecatedColourList.insert(wxSYS_COLOUR_BACKGROUND);
    m_deprecatedColourList.insert(wxSYS_COLOUR_BTNFACE);
    m_deprecatedColourList.insert(wxSYS_COLOUR_BTNHIGHLIGHT);
    m_deprecatedColourList.insert(wxSYS_COLOUR_BTNHILIGHT);
    m_deprecatedColourList.insert(wxSYS_COLOUR_BTNSHADOW);
    m_deprecatedColourList.insert(wxSYS_COLOUR_CAPTIONTEXT);
    m_deprecatedColourList.insert(wxSYS_COLOUR_DESKTOP);
    m_deprecatedColourList.insert(wxSYS_COLOUR_FRAMEBK);
    m_deprecatedColourList.insert(wxSYS_COLOUR_GRADIENTACTIVECAPTION);
    m_deprecatedColourList.insert(wxSYS_COLOUR_GRADIENTINACTIVECAPTION);
    m_deprecatedColourList.insert(wxSYS_COLOUR_INACTIVEBORDER);
    m_deprecatedColourList.insert(wxSYS_COLOUR_INACTIVECAPTION);
    m_deprecatedColourList.insert(wxSYS_COLOUR_INACTIVECAPTIONTEXT);
    m_deprecatedColourList.insert(wxSYS_COLOUR_INFOBK);
    m_deprecatedColourList.insert(wxSYS_COLOUR_INFOTEXT);
    m_deprecatedColourList.insert(wxSYS_COLOUR_MENU);
    m_deprecatedColourList.insert(wxSYS_COLOUR_MENUBAR);
    m_deprecatedColourList.insert(wxSYS_COLOUR_MENUHILIGHT);
    m_deprecatedColourList.insert(wxSYS_COLOUR_MENUTEXT);
    m_deprecatedColourList.insert(wxSYS_COLOUR_SCROLLBAR);
    m_deprecatedColourList.insert(wxSYS_COLOUR_WINDOWFRAME);
#endif
}

bool SystemColourView::IsDeprecatedSystemColour(wxSystemColour index) const
{
#ifndef __WXMSW__
    wxUnusedVar(index);
    return false;
#else
    return m_deprecatedColourList.find(index) != m_deprecatedColourList.end();
#endif
}


/*************************************************

    SystemFontView

*************************************************/

class SystemFontView : public SystemSettingView
{
public:
    SystemFontView(wxWindow* parent);

    bool CanShowDetailedInformation() const override { return GetFirstSelected() != -1; }

protected:
    void DoUpdateValues() override;
    void DoShowDetailedInformation(long listItemIndex) const override;
};

struct FontInfo
{
    wxSystemFont index;
    const char*  name;
    const char*  description;
};

FontInfo const s_fontInfoArray[] =
{
    { wxSYS_OEM_FIXED_FONT, "wxSYS_OEM_FIXED_FONT", "Original equipment manufacturer dependent fixed-pitch font." },
    { wxSYS_ANSI_FIXED_FONT, "wxSYS_ANSI_FIXED_FONT", "Windows fixed-pitch (monospaced) font." },
    { wxSYS_ANSI_VAR_FONT, "wxSYS_ANSI_VAR_FONT", "Windows variable-pitch (proportional) font." },
    { wxSYS_SYSTEM_FONT, "wxSYS_SYSTEM_FONT", "System font. By default, the system uses the system font to draw menus, dialog box controls, and text." },
    { wxSYS_DEVICE_DEFAULT_FONT, "wxSYS_DEVICE_DEFAULT_FONT", "Device-dependent font." },
    { wxSYS_DEFAULT_GUI_FONT, "wxSYS_DEFAULT_GUI_FONT", "Default font for user interface objects such as menus and dialog boxes." },
};

SystemFontView::SystemFontView(wxWindow* parent)
    : SystemSettingView(parent)
{
    for ( size_t i = 0; i < WXSIZEOF(s_fontInfoArray); ++i )
    {
         const wxString fontName = s_fontInfoArray[i].name;
         const wxString fontDescription = s_fontInfoArray[i].description;
         const long itemIndex = AppendItemWithData(fontName, (long)i);

         if ( itemIndex != -1 )
            SetItem(itemIndex, Column_Description, fontDescription);
    }

    UpdateValues();
}

void SystemFontView::DoUpdateValues()
{
    const int itemCount = GetItemCount();

    wxLogNull logNo;

    for ( int i = 0; i < itemCount; ++i )
    {
         const wxFont font = wxSystemSettings::GetFont(s_fontInfoArray[GetItemData(i)].index);
         wxString fontValue = _("<Invalid>");

         if ( font.IsOk() )
              fontValue = font.GetNativeFontInfoUserDesc();

         SetItem(i, Column_Value, fontValue);
    }
}

void SystemFontView::DoShowDetailedInformation(long listItemIndex) const
{
    const size_t infoArrayIndex = GetItemData(listItemIndex);

    const wxString valueName = s_fontInfoArray[infoArrayIndex].name;
    const wxFont font = wxSystemSettings::GetFont(s_fontInfoArray[infoArrayIndex].index);

    if ( !font.IsOk() )
    {
        wxLogError(_("Invalid font for \"%s\"."), valueName);
        return;
    }

    wxGetFontFromUser(wxGetTopLevelParent(const_cast<SystemFontView*>(this)), font, wxString::Format(_("Viewing %s"), valueName));
}


/*************************************************

    SystemMetricView

*************************************************/

class SystemMetricView : public SystemSettingView
{
public:
    SystemMetricView(wxWindow* parent);
protected:
    void DoUpdateValues() override;
};


struct MetricInfo
{
    wxSystemMetric index;
    const char*    name;
    const char*    description;
};

MetricInfo const s_metricInfoArray[] =
{
    { wxSYS_MOUSE_BUTTONS, "wxSYS_MOUSE_BUTTONS", "Number of buttons on mouse, or zero if no mouse was installed." },
    { wxSYS_BORDER_X, "wxSYS_BORDER_X", "Width of single border." },
    { wxSYS_BORDER_Y, "wxSYS_BORDER_Y", "Height of single border." },
    { wxSYS_CURSOR_X, "wxSYS_CURSOR_X", "Width of cursor." },
    { wxSYS_CURSOR_Y, "wxSYS_CURSOR_Y", "Height of cursor." },
    { wxSYS_DCLICK_X, "wxSYS_DCLICK_X", "Width in pixels of rectangle within which two successive mouse clicks must fall to generate a double-click." },
    { wxSYS_DCLICK_Y, "wxSYS_DCLICK_Y", "Height in pixels of rectangle within which two successive mouse clicks must fall to generate a double-click." },
    { wxSYS_DRAG_X, "wxSYS_DRAG_X", "Width in pixels of a rectangle centered on a drag point to allow for limited movement of the mouse pointer before a drag operation begins." },
    { wxSYS_DRAG_Y, "wxSYS_DRAG_Y", "Height in pixels of a rectangle centered on a drag point to allow for limited movement of the mouse pointer before a drag operation begins." },
    { wxSYS_EDGE_X, "wxSYS_EDGE_X", "Width of a 3D border, in pixels." },
    { wxSYS_EDGE_Y, "wxSYS_EDGE_Y", "Height of a 3D border, in pixels." },
    { wxSYS_HSCROLL_ARROW_X, "wxSYS_HSCROLL_ARROW_X", "Width of arrow bitmap on horizontal scrollbar." },
    { wxSYS_HSCROLL_ARROW_Y, "wxSYS_HSCROLL_ARROW_Y", "Height of arrow bitmap on horizontal scrollbar." },
    { wxSYS_HTHUMB_X, "wxSYS_HTHUMB_X", "Width of horizontal scrollbar thumb." },
    { wxSYS_ICON_X, "wxSYS_ICON_X", "The default width of an icon." },
    { wxSYS_ICON_Y, "wxSYS_ICON_Y", "The default height of an icon." },
    { wxSYS_ICONSPACING_X, "wxSYS_ICONSPACING_X", "Width of a grid cell for items in large icon view, in pixels. Each item fits into a rectangle of this size when arranged." },
    { wxSYS_ICONSPACING_Y, "wxSYS_ICONSPACING_Y", "Height of a grid cell for items in large icon view, in pixels. Each item fits into a rectangle of this size when arranged." },
    { wxSYS_WINDOWMIN_X, "wxSYS_WINDOWMIN_X", "Minimum width of a window." },
    { wxSYS_WINDOWMIN_Y, "wxSYS_WINDOWMIN_Y", "Minimum height of a window." },
    { wxSYS_SCREEN_X, "wxSYS_SCREEN_X", "Width of the screen in pixels." },
    { wxSYS_SCREEN_Y, "wxSYS_SCREEN_Y", "Height of the screen in pixels." },
    { wxSYS_FRAMESIZE_X, "wxSYS_FRAMESIZE_X", "Width of the window frame for a wxTHICK_FRAME window." },
    { wxSYS_FRAMESIZE_Y, "wxSYS_FRAMESIZE_Y", "Height of the window frame for a wxTHICK_FRAME window." },
    { wxSYS_SMALLICON_X, "wxSYS_SMALLICON_X", "Recommended width of a small icon (in window captions, and small icon view)." },
    { wxSYS_SMALLICON_Y, "wxSYS_SMALLICON_Y", "Recommended height of a small icon (in window captions, and small icon view)." },
    { wxSYS_HSCROLL_Y, "wxSYS_HSCROLL_Y", "Height of horizontal scrollbar in pixels." },
    { wxSYS_VSCROLL_X, "wxSYS_VSCROLL_X", "Width of vertical scrollbar in pixels." },
    { wxSYS_VSCROLL_ARROW_X, "wxSYS_VSCROLL_ARROW_X", "Width of arrow bitmap on a vertical scrollbar." },
    { wxSYS_VSCROLL_ARROW_Y, "wxSYS_VSCROLL_ARROW_Y", "Height of arrow bitmap on a vertical scrollbar." },
    { wxSYS_VTHUMB_Y, "wxSYS_VTHUMB_Y", "Height of vertical scrollbar thumb." },
    { wxSYS_CAPTION_Y, "wxSYS_CAPTION_Y", "Height of normal caption area." },
    { wxSYS_MENU_Y, "wxSYS_MENU_Y", "Height of single-line menu bar." },
    { wxSYS_NETWORK_PRESENT, "wxSYS_NETWORK_PRESENT", "1 if there is a network present, 0 otherwise." },
    { wxSYS_PENWINDOWS_PRESENT, "wxSYS_PENWINDOWS_PRESENT", "1 if PenWindows is installed, 0 otherwise." },
    { wxSYS_SHOW_SOUNDS, "wxSYS_SHOW_SOUNDS", "Non-zero if the user requires an application to present information visually in situations where it would otherwise present the information only in audible form; zero otherwise." },
    { wxSYS_SWAP_BUTTONS, "wxSYS_SWAP_BUTTONS", "Non-zero if the meanings of the left and right mouse buttons are swapped; zero otherwise." },
    { wxSYS_DCLICK_MSEC, "wxSYS_DCLICK_MSEC", "Maximal time, in milliseconds, which may pass between subsequent clicks for a double click to be generated." },

// there was a bug with these three metrics on wxMSW
// until commit e5d850d (commited on Apr 18, 2019)
#if wxCHECK_VERSION(3, 1, 1) && ( !defined(__WXMSW__) || wxCHECK_VERSION(3, 1, 3)  )
    { wxSYS_CARET_ON_MSEC, "wxSYS_CARET_ON_MSEC", "Time, in milliseconds, for how long a blinking caret should stay visible during a single blink cycle before it disappears." },
    { wxSYS_CARET_OFF_MSEC, "wxSYS_CARET_OFF_MSEC", "Time, in milliseconds, for how long a blinking caret should stay invisible during a single blink cycle before it reappears. If this value is zero, carets should be visible all the time instead of blinking. If the value is negative, the platform does not support the user setting. Implemented only on GTK+ and MacOS X." },
    { wxSYS_CARET_TIMEOUT_MSEC, "wxSYS_CARET_TIMEOUT_MSEC", "Time, in milliseconds, for how long a caret should blink after a user interaction. After this timeout has expired, the caret should stay continuously visible until the user interacts with the caret again (for example by entering, deleting or cutting text). If this value is negative, carets should blink forever; if it is zero, carets should not blink at all." },
#endif // #if wxCHECK_VERSION(3, 1, 1) && ( !defined(__WXMSW__) || wxCHECK_VERSION(3, 1, 3)  )
};

SystemMetricView::SystemMetricView(wxWindow* parent)
    : SystemSettingView(parent)
{
    for ( size_t i = 0; i < WXSIZEOF(s_metricInfoArray); ++i )
    {
         const wxString metricName = s_metricInfoArray[i].name;
         const wxString metricDescription = s_metricInfoArray[i].description;
         const long itemIndex = AppendItemWithData(metricName, (long)i);

         if ( itemIndex != -1 )
            SetItem(itemIndex, Column_Description, metricDescription);
    }

    m_columnWidths[Column_Value] = wxLIST_AUTOSIZE_USEHEADER;

    UpdateValues();
}

void SystemMetricView::DoUpdateValues()
{
    const int itemCount = GetItemCount();

    for ( int i = 0; i < itemCount; ++i )
    {
         const int metricValue  = wxSystemSettings::GetMetric(s_metricInfoArray[GetItemData(i)].index, wxGetTopLevelParent(this));

         SetItem(i, Column_Value, wxString::Format("%d", metricValue));
    }
}


/*************************************************

    DisplaysView

*************************************************/

#ifdef __WXMSW__

// The code for obtaining friendly monitor names is adapted from
// https://gist.github.com/pavel-a/dd3a4320176e69a0f6c4b4871e69e56b

// Returns the monitor name stored in the EDID, which can be longer than what
// DisplayConfigGetDeviceInfo() reports: a single EDID monitor name descriptor holds
// only 13 characters, so some monitors store longer names in several descriptors
// that need to be concatenated.
wxString GetMonitorNameFromEDID(const std::vector<BYTE>& edid)
{
    wxString name;

    const auto addDescriptor = [&name](const BYTE* d)
    {
        const BYTE monitorNameTag = 0xFC;

        // display descriptors (as opposed to detailed timing descriptors)
        // start with 3 zero bytes followed by the tag, the text is at offset 5
        if ( d[0] != 0 || d[1] != 0 || d[2] != 0 || d[3] != monitorNameTag )
            return;

        for ( int i = 5; i < 18 && d[i] != 0x0A && d[i] != 0; ++i )
            name += wxUniChar(d[i]);
    };

    if ( edid.size() < 128 )
        return name;

    // the base block contains four 18-byte descriptors
    for ( size_t offset = 54; offset <= 108; offset += 18 )
        addDescriptor(&edid[offset]);

    // CTA-861 extension blocks can contain more descriptors after the data blocks
    const size_t extensionCount = edid[126];

    for ( size_t i = 1; i <= extensionCount && edid.size() >= (i + 1) * 128; ++i )
    {
        const BYTE* const block = &edid[i * 128];

        if ( block[0] != 0x02 || block[2] < 4 )
            continue;

        for ( size_t offset = block[2]; offset + 18 <= 127; offset += 18 )
            addDescriptor(block + offset);
    }

    return name.Trim();
}

// Reads the EDID of the monitor identified by the monitor device path
// returned by DisplayConfigGetDeviceInfo(), e.g.
// \\?\DISPLAY#GBT2701#5&9d4e723&0&UID37122#{e6f07b5f-ee97-4a90-b076-33f57bf4eaa7}
bool ReadMonitorEDID(const wxString& monitorDevicePath, std::vector<BYTE>& edid)
{
    const wxArrayString parts = wxSplit(monitorDevicePath, '#', '\0');

    if ( parts.size() < 3 )
        return false;

    const wxString key = wxString::Format(
        "SYSTEM\\CurrentControlSet\\Enum\\DISPLAY\\%s\\%s\\Device Parameters", parts[1], parts[2]);

    DWORD size = 0;

    if ( ::RegGetValueW(HKEY_LOCAL_MACHINE, key.wc_str(), L"EDID", RRF_RT_REG_BINARY,
                        nullptr, nullptr, &size) != ERROR_SUCCESS || !size )
        return false;

    edid.resize(size);

    if ( ::RegGetValueW(HKEY_LOCAL_MACHINE, key.wc_str(), L"EDID", RRF_RT_REG_BINARY,
                        nullptr, edid.data(), &size) != ERROR_SUCCESS )
        return false;

    edid.resize(size);
    return true;
}

// Returns friendly monitor names keyed by the GDI device name
// (as returned by wxDisplay::GetName()).
std::map<wxString, wxString> GetMonitorFriendlyNames()
{
    std::map<wxString, wxString> names;
    std::vector<DISPLAYCONFIG_PATH_INFO> paths;
    std::vector<DISPLAYCONFIG_MODE_INFO> modes;
    LONG result;

    // the display configuration may change between the two calls,
    // in which case QueryDisplayConfig() asks to be called again
    do
    {
        UINT32 pathCount = 0, modeCount = 0;

        if ( ::GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount) != ERROR_SUCCESS )
            return names;

        paths.resize(pathCount);
        modes.resize(modeCount);

        result = ::QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS,
                    &pathCount, paths.data(), &modeCount, modes.data(), nullptr);

        if ( result == ERROR_SUCCESS )
            paths.resize(pathCount);
    }
    while ( result == ERROR_INSUFFICIENT_BUFFER );

    if ( result != ERROR_SUCCESS )
        return names;

    for ( const auto& p : paths )
    {
        DISPLAYCONFIG_SOURCE_DEVICE_NAME sourceName{};

        sourceName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
        sourceName.header.size = sizeof(sourceName);
        sourceName.header.adapterId = p.sourceInfo.adapterId;
        sourceName.header.id = p.sourceInfo.id;
        if ( ::DisplayConfigGetDeviceInfo(&sourceName.header) != ERROR_SUCCESS )
            continue;

        DISPLAYCONFIG_TARGET_DEVICE_NAME targetName{};

        targetName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
        targetName.header.size = sizeof(targetName);
        targetName.header.adapterId = p.targetInfo.adapterId;
        targetName.header.id = p.targetInfo.id;
        if ( ::DisplayConfigGetDeviceInfo(&targetName.header) != ERROR_SUCCESS )
            continue;

        wxString friendlyName(targetName.monitorFriendlyDeviceName);

        // prefer the full name from EDID if the reported name is just its truncated beginning
        std::vector<BYTE> edid;

        if ( targetName.flags.friendlyNameFromEdid
             && ReadMonitorEDID(targetName.monitorDevicePath, edid) )
        {
            const wxString edidName = GetMonitorNameFromEDID(edid);

            if ( edidName.length() > friendlyName.length()
                 && edidName.Left(friendlyName.length()).CmpNoCase(friendlyName) == 0 )
            {
                friendlyName = edidName;
            }
        }

        names[wxString(sourceName.viewGdiDeviceName)] = friendlyName;
    }

    return names;
}

#endif // #ifdef __WXMSW__


class DisplaysView : public SysInfoListView
{
public:
    DisplaysView(wxWindow* parent);

    wxArrayString GetValues(const wxString& separator) const override;

protected:
    void DoUpdateValues() override;
private:
    enum
    {
        Param_Name = 0,
        Param_FriendlyName,
        Param_IsPrimary,
        Param_Resolution,
        Param_BPP,
        Param_Frequency,
        Param_GeometryCoords,
        Param_GeometrySize,
        Param_ClientAreaCoords,
        Param_ClientAreaSize,
        Param_PPI,
        Param_HasThisWindow
    };
};

DisplaysView::DisplaysView(wxWindow* parent)
    : SysInfoListView(parent)
{
    AppendColumn(_("Parameter"));

    AppendItemWithData(_("Name"), Param_Name);
#ifdef __WXMSW__
    AppendItemWithData(_("Friendly Name"), Param_FriendlyName);
#endif
    AppendItemWithData(_("Is Primary"), Param_IsPrimary);
    AppendItemWithData(_("Resolution"), Param_Resolution);
    AppendItemWithData(_("Bits Per Pixel"), Param_BPP);
    AppendItemWithData(_("Refresh Frequency (Hz)"), Param_Frequency);
    AppendItemWithData(_("Geometry Coordinates (left, top; right, bottom)"), Param_GeometryCoords);
    AppendItemWithData(_("Geometry Size"), Param_GeometrySize);
    AppendItemWithData(_("Client Area Coordinates (left, top; right, bottom)"), Param_ClientAreaCoords);
    AppendItemWithData(_("Client Area Size"), Param_ClientAreaSize);
    AppendItemWithData(_("Pixels Per Inch"), Param_PPI);
    AppendItemWithData(_("Has This Window"), Param_HasThisWindow);

    UpdateValues();
}

wxArrayString DisplaysView::GetValues(const wxString& separator) const
{
    const int itemCount = GetItemCount();
    const int columnCount = GetColumnCount();

    wxArrayString values;
    wxString s;

    values.reserve(itemCount + 1);

    // column headings
    s = _("Parameter");

    for ( int columnIndex = 1; columnIndex < columnCount; ++columnIndex )
    {
        wxListItem listItem;

        listItem.SetMask(wxLIST_MASK_TEXT);
        GetColumn(columnIndex, listItem);
        s += separator + listItem.GetText();
    }
    values.push_back(s);

    // dump values
    for ( int itemIndex = 0; itemIndex < itemCount; ++itemIndex )
    {
        s = GetItemText(itemIndex, 0);
        for ( int columnIndex = 1; columnIndex < columnCount; ++columnIndex )
        {
            s += separator + GetItemText(itemIndex, columnIndex);
        }
        values.push_back(s);
    }

    return values;
}

void DisplaysView::DoUpdateValues()
{
    while ( GetColumnCount() > 1 )
        DeleteColumn(1);

    const unsigned int displayCount = wxDisplay::GetCount();
    const int displayForThisWindow = wxDisplay::GetFromWindow(wxGetTopLevelParent(this));
    const int itemCount = GetItemCount();

#ifdef __WXMSW__
    const std::map<wxString, wxString> friendlyNames = GetMonitorFriendlyNames();
#endif // #ifdef __WXMSW__

    for ( unsigned int displayIndex = 0; displayIndex < displayCount; ++displayIndex )
    {
        const wxDisplay display(displayIndex);
        const wxVideoMode videoMode = display.GetCurrentMode();
        const wxRect geometryCoords = display.GetGeometry();
        const wxRect clientAreaCoords = display.GetClientArea();
        const int columnIndex = displayIndex + 1;

        AppendColumn(wxString::Format("wxDisplay(%u)", displayIndex));

        for ( int itemIndex = 0; itemIndex < itemCount; ++itemIndex )
        {
            const long param = GetItemData(itemIndex);
            wxString value;

            switch ( param )
            {
                case Param_Name:
                    value = display.GetName();
                    break;
#ifdef __WXMSW__
                case Param_FriendlyName:
                    {
                        const auto it = friendlyNames.find(display.GetName());

                        if ( it != friendlyNames.end() && !it->second.empty() )
                            value = it->second;
                        else
                            value = _("N/A");
                    }
                    break;
#endif // #ifdef __WXMSW__
                case Param_IsPrimary:
                    value =  display.IsPrimary() ? _("Yes") : _("No");
                    break;
                case Param_Resolution:
                    value = wxSizeTowxString(wxSize(videoMode.GetWidth(), videoMode.GetHeight()));
                    break;
                case Param_BPP:
                    value.Printf("%d", videoMode.GetDepth());
                    break;
                case Param_Frequency:
                    value.Printf("%d", videoMode.refresh);
                    break;
                case Param_GeometryCoords:
                    value = wxRectTowxString(geometryCoords);
                    break;
                case Param_GeometrySize:
                    value = wxSizeTowxString(geometryCoords.GetSize());
                    break;
                case Param_ClientAreaCoords:
                    value = wxRectTowxString(clientAreaCoords);
                    break;
                case Param_ClientAreaSize:
                    value = wxSizeTowxString(clientAreaCoords.GetSize());
                    break;
                case Param_PPI:
                    value = wxSizeTowxString(display.GetPPI());
                    break;
                case Param_HasThisWindow:
                    value = displayForThisWindow == static_cast<int>(displayIndex) ? _("Yes") : _("No");
                    break;
                default:
                    wxFAIL;
            }

            SetItem(itemIndex, columnIndex, value);
        }
    }
}


/*************************************************

    SystemOptionsView

*************************************************/

#if wxUSE_SYSTEM_OPTIONS

class SystemOptionsView : public SysInfoListView
{
public:
    SystemOptionsView(wxWindow* parent);

    wxArrayString GetValues(const wxString& separator) const override
    {
        return GetNameAndValueValues(Column_Name, Column_Value, separator);
    }

protected:
    void DoUpdateValues() override;
private:
    enum
    {
        Column_Name = 0,
        Column_Value,
    };

    static const char* s_optionNames[];
};

const char* SystemOptionsView::s_optionNames[] =
{
    "exit-on-assert",
    "catch-unhandled-exceptions",
#if defined(__WXMSW__)
    "msw.dark-mode",
    "msw.font.no-proof-quality",
    "msw.native-dialogs-pmdpi",
    "msw.no-manifest-check",
    "msw.notebook.themed-background",
    "msw.remap",
    "msw.staticbox.optimized-paint",
    "msw.window.no-clip-children",
    "msw.window.no-composited",
    "no-maskblt",
#elif defined(__WXGTK__)
    "gtk.desktop",
    "gtk.tlw.can-set-transparent",
    "gtk.window.force-background-colour",
#elif defined(__WXMAC__)
    "mac.listctrl.always_use_generic",
    "mac.textcontrol-use-spell-checker",
    "window-default-variant",
    "mac.window-plain-transition",
    "osx.openfiledialog.always-show-types",
#endif
};

SystemOptionsView::SystemOptionsView(wxWindow* parent)
    : SysInfoListView(parent)
{
    InsertColumn(Column_Name, _("Name"));
    InsertColumn(Column_Value, _("Value"));

    UpdateValues();
}

wxString SysOptToString(const wxString& name)
{
    if ( wxSystemOptions::HasOption(name) )
        return wxSystemOptions::GetOption(name);

    return _("<Not Set>");
}

void SystemOptionsView::DoUpdateValues()
{
    DeleteAllItems();

    for ( const auto& name : s_optionNames )
    {
        const long itemIndex = InsertItem(GetItemCount(), name);

         if ( itemIndex != -1 )
            SetItem(itemIndex, Column_Value, SysOptToString(name));
    }
}

#endif // #if wxUSE_SYSTEM_OPTIONS


/*************************************************

    StandardPathsView

*************************************************/

class StandardPathsView : public SysInfoListView
{
public:
    StandardPathsView(wxWindow* parent);

    wxArrayString GetValues(const wxString& separator) const override
    {
        return GetNameAndValueValues(Column_Name, Column_Value, separator);
    }
protected:
    void DoUpdateValues() override;
private:
    enum
    {
        Column_Name = 0,
        Column_Value,
    };

    enum
    {
        Param_ExecutablePath = 0,
        Param_AppDocumentsDir,
        Param_ConfigDir,
        Param_DataDir,
        Param_DocumentsDir,
        Param_LocalDataDir,
        Param_PluginsDir,
        Param_ResourcesDir,
        Param_TempDir,
        Param_UserConfigDir,
        Param_UserDataDir,
        Param_UserLocalDataDir,

        // the group below is available only for wxWidgets 3.1+
        Param_UserDir_Cache,
        Param_UserDir_Documents,
        Param_UserDir_Desktop,
        Param_UserDir_Downloads,
        Param_UserDir_Music,
        Param_UserDir_Pictures,
        Param_UserDir_Videos,

        // GTK only
        Param_InstallPrefix,

        // wxMSW only: the item data for the shell directories is
        // Param_FirstShellDir + index in s_shellDirs
        Param_FirstShellDir = 1000
    };

#ifdef __WXMSW__
    struct ShellDirInfo
    {
        const char* name;
        int         csidl;
    };

    static const ShellDirInfo s_shellDirs[];
#endif // #ifdef __WXMSW__
};

#ifdef __WXMSW__

const StandardPathsView::ShellDirInfo StandardPathsView::s_shellDirs[] =
{
    { "CSIDL_DESKTOP", 0x0000 },
    { "CSIDL_INTERNET", 0x0001 },
    { "CSIDL_PROGRAMS", 0x0002 },
    { "CSIDL_CONTROLS", 0x0003 },
    { "CSIDL_PRINTERS", 0x0004 },
    { "CSIDL_FAVORITES", 0x0006 },
    { "CSIDL_STARTUP", 0x0007 },
    { "CSIDL_RECENT", 0x0008 },
    { "CSIDL_SENDTO", 0x0009 },
    { "CSIDL_BITBUCKET", 0x000a },
    { "CSIDL_STARTMENU", 0x000b },
    { "CSIDL_MYDOCUMENTS", 0x0005 },
    { "CSIDL_MYMUSIC", 0x000d },
    { "CSIDL_MYVIDEO", 0x000e },
    { "CSIDL_DESKTOPDIRECTORY", 0x0010 },
    { "CSIDL_DRIVES", 0x0011 },
    { "CSIDL_NETWORK", 0x0012 },
    { "CSIDL_NETHOOD", 0x0013 },
    { "CSIDL_FONTS", 0x0014 },
    { "CSIDL_TEMPLATES", 0x0015 },
    { "CSIDL_COMMON_STARTMENU", 0x0016 },
    { "CSIDL_COMMON_PROGRAMS", 0x0017 },
    { "CSIDL_COMMON_STARTUP", 0x0018 },
    { "CSIDL_COMMON_DESKTOPDIRECTORY", 0x0019 },
    { "CSIDL_APPDATA", 0x001a },
    { "CSIDL_PRINTHOOD", 0x001b },
    { "CSIDL_LOCAL_APPDATA", 0x001c },
    { "CSIDL_ALTSTARTUP", 0x001d },
    { "CSIDL_COMMON_ALTSTARTUP", 0x001e },
    { "CSIDL_COMMON_FAVORITES", 0x001f },
    { "CSIDL_INTERNET_CACHE", 0x0020 },
    { "CSIDL_COOKIES", 0x0021 },
    { "CSIDL_HISTORY", 0x0022 },
    { "CSIDL_COMMON_APPDATA", 0x0023 },
    { "CSIDL_WINDOWS", 0x0024 },
    { "CSIDL_SYSTEM", 0x0025 },
    { "CSIDL_PROGRAM_FILES", 0x0026 },
    { "CSIDL_MYPICTURES", 0x0027 },
    { "CSIDL_PROFILE", 0x0028 },
    { "CSIDL_SYSTEMX86", 0x0029 },
    { "CSIDL_PROGRAM_FILESX86", 0x002a },
    { "CSIDL_PROGRAM_FILES_COMMON", 0x002b },
    { "CSIDL_PROGRAM_FILES_COMMONX86", 0x002c },
    { "CSIDL_COMMON_TEMPLATES", 0x002d },
    { "CSIDL_COMMON_DOCUMENTS", 0x002e },
    { "CSIDL_COMMON_ADMINTOOLS", 0x002f },
    { "CSIDL_ADMINTOOLS", 0x0030 },
    { "CSIDL_CONNECTIONS", 0x0031 },
    { "CSIDL_COMMON_MUSIC", 0x0035 },
    { "CSIDL_COMMON_PICTURES", 0x0036 },
    { "CSIDL_COMMON_VIDEO", 0x0037 },
    { "CSIDL_RESOURCES", 0x0038 },
    { "CSIDL_RESOURCES_LOCALIZED", 0x0039 },
    { "CSIDL_COMMON_OEM_LINKS", 0x003a },
    { "CSIDL_COMPUTERSNEARME", 0x003d },
};

#endif // #ifdef __WXMSW__

StandardPathsView::StandardPathsView(wxWindow* parent)
    : SysInfoListView(parent)
{
    InsertColumn(Column_Name, _("Name"));
    InsertColumn(Column_Value, _("Value"));

    AppendItemWithData("ExecutablePath", Param_ExecutablePath);
    AppendItemWithData("AppDocumentsDir", Param_AppDocumentsDir);

    AppendItemWithData("ConfigDir", Param_ConfigDir);
    AppendItemWithData("DataDir", Param_DataDir);
    AppendItemWithData("DocumentsDir", Param_DocumentsDir);
    AppendItemWithData("LocalDataDir", Param_LocalDataDir);
    AppendItemWithData("PluginsDir", Param_PluginsDir);
    AppendItemWithData("ResourcesDir", Param_ResourcesDir);
    AppendItemWithData("TempDir", Param_TempDir);
    AppendItemWithData("UserConfigDir", Param_UserConfigDir);
    AppendItemWithData("UserDataDir", Param_UserDataDir);
    AppendItemWithData("UserLocalDataDir", Param_UserLocalDataDir);

#if wxCHECK_VERSION(3, 1, 0)
    AppendItemWithData("UserDir_Cache", Param_UserDir_Cache);
    AppendItemWithData("UserDir_Documents", Param_UserDir_Documents);
    AppendItemWithData("UserDir_Desktop", Param_UserDir_Desktop);
    AppendItemWithData("UserDir_Downloads", Param_UserDir_Downloads);
    AppendItemWithData("UserDir_Music", Param_UserDir_Music);
    AppendItemWithData("UserDir_Pictures", Param_UserDir_Pictures);
    AppendItemWithData("UserDir_Videos", Param_UserDir_Videos);
#endif // #if wxCHECK_VERSION(3, 1, 0)

#ifdef __WXMSW__
    for ( size_t i = 0; i < WXSIZEOF(s_shellDirs); ++i )
    {
        AppendItemWithData(wxString("MSWShellDir ") + s_shellDirs[i].name,
                           Param_FirstShellDir + (long)i);
    }
#endif // #ifdef __WXMSW__

#ifdef __WXGTK__
    AppendItemWithData("InstallPrefix", Param_InstallPrefix);
#endif // #ifdef __WXGTK__

    UpdateValues();
}

void StandardPathsView::DoUpdateValues()
{
    const wxStandardPaths& paths = wxStandardPaths::Get();
    const int itemCount = GetItemCount();

#ifdef __WXMSW__
    // for MSWGetShellDir()
    // CSIDL_FLAG_DONT_VERIFY | CSIDL_FLAG_DONT_UNEXPAND | CSIDL_FLAG_NO_ALIAS
    const UINT flags =  0x4000 | 0x2000 | 0x1000;
#endif // #ifdef __WXMSW__

    for ( int i = 0; i < itemCount; ++i )
    {
        const long param = GetItemData(i);
        wxString value;

#ifdef __WXMSW__
        if ( param >= Param_FirstShellDir )
        {
            value = paths.MSWGetShellDir(s_shellDirs[param - Param_FirstShellDir].csidl | flags);
            SetItem(i, Column_Value, value);
            continue;
        }
#endif // #ifdef __WXMSW__

        switch ( param )
        {
            case Param_ExecutablePath:   value = paths.GetExecutablePath(); break;
            case Param_AppDocumentsDir:  value = paths.GetAppDocumentsDir(); break;
            case Param_ConfigDir:        value = paths.GetConfigDir(); break;
            case Param_DataDir:          value = paths.GetDataDir(); break;
            case Param_DocumentsDir:     value = paths.GetDocumentsDir(); break;
            case Param_LocalDataDir:     value = paths.GetLocalDataDir(); break;
            case Param_PluginsDir:       value = paths.GetPluginsDir(); break;
            case Param_ResourcesDir:     value = paths.GetResourcesDir(); break;
            case Param_TempDir:          value = paths.GetTempDir(); break;
            case Param_UserConfigDir:    value = paths.GetUserConfigDir(); break;
            case Param_UserDataDir:      value = paths.GetUserDataDir(); break;
            case Param_UserLocalDataDir: value = paths.GetUserLocalDataDir(); break;

#if wxCHECK_VERSION(3, 1, 0)
            case Param_UserDir_Cache:     value = paths.GetUserDir(wxStandardPaths::Dir_Cache); break;
            case Param_UserDir_Documents: value = paths.GetUserDir(wxStandardPaths::Dir_Documents); break;
            case Param_UserDir_Desktop:   value = paths.GetUserDir(wxStandardPaths::Dir_Desktop); break;
            case Param_UserDir_Downloads: value = paths.GetUserDir(wxStandardPaths::Dir_Downloads); break;
            case Param_UserDir_Music:     value = paths.GetUserDir(wxStandardPaths::Dir_Music); break;
            case Param_UserDir_Pictures:  value = paths.GetUserDir(wxStandardPaths::Dir_Pictures); break;
            case Param_UserDir_Videos:    value = paths.GetUserDir(wxStandardPaths::Dir_Videos); break;
#endif // #if wxCHECK_VERSION(3, 1, 0)

#ifdef __WXGTK__
            case Param_InstallPrefix:  value = paths.GetInstallPrefix(); break;
#endif // #ifdef __WXGTK__

            default:
                wxFAIL;
        }

        SetItem(i, Column_Value, value);
    }
}


/*************************************************

    EnvironmentVariablesView

*************************************************/

class EnvironmentVariablesView : public SysInfoListView
{
public:
    EnvironmentVariablesView(wxWindow* parent);

    wxArrayString GetValues(const wxString& separator) const override
    {
        return GetNameAndValueValues(Column_Name, Column_Value, separator);
    }

protected:
    void DoUpdateValues() override;
private:
    enum
    {
        Column_Name = 0,
        Column_Value,
    };
};

EnvironmentVariablesView::EnvironmentVariablesView(wxWindow* parent)
    : SysInfoListView(parent)
{
    InsertColumn(Column_Name, _("Name"));
    InsertColumn(Column_Value, _("Value"));

    UpdateValues();
}

void EnvironmentVariablesView::DoUpdateValues()
{
    DeleteAllItems();

    wxEnvVariableHashMap variables;

    if ( !wxGetEnvMap(&variables) )
    {
        wxLogError(_("Could not retrieve system environment variables."));
        return;
    }

    // sort variables alphabetically by name, ignoring case
    // as environment variable names are case-insensitive on Windows
    struct NoCaseLess
    {
        bool operator()(const wxString& a, const wxString& b) const
        {
            const int cmp = a.CmpNoCase(b);

            return cmp < 0 || (cmp == 0 && a < b);
        }
    };

    std::map<wxString, wxString, NoCaseLess> variablesSorted;

    for ( auto i = variables.begin(); i != variables.end(); ++i )
        variablesSorted[i->first] = i->second;

    for ( const auto& variable : variablesSorted )
    {
         const long itemIndex = InsertItem(GetItemCount(), variable.first);

         if ( itemIndex != -1 )
            SetItem(itemIndex, Column_Value, variable.second);
    }
}


/*************************************************

    MiscellaneousView

*************************************************/

// State shared between MiscellaneousView and the (detached) thread obtaining the full
// host name, which can take a long time. The view never waits for the thread: it just
// invalidates the request and the thread then discards its result.
struct HostNameLookupState
{
    wxCriticalSection cs;
    wxEvtHandler*     sink{nullptr};
    unsigned          requestId{0};
};

class MiscellaneousView : public SysInfoListView
{
public:
    MiscellaneousView(wxWindow* parent);
    ~MiscellaneousView();

    wxArrayString GetValues(const wxString& separator) const override
    {
        return GetNameAndValueValues(Column_Name, Column_Value, separator);
    }

protected:
    void DoUpdateValues() override;
private:
    enum
    {
        Column_Name = 0,
        Column_Value,
    };

    enum
    {
        Param_AppName = 0,
        Param_AppDisplayName,
        Param_AppVendorName,
        Param_AppVendorDisplayName,
        Param_AppClassName,
        Param_AppHasStderr,
        Param_IsProcess64bit,
        Param_wxRCEmbedded,
        Param_IsRemoteSession,
        Param_UnixDesktopEnvironment,
        Param_ThemeName,
        Param_SystemAppearanceName,
        Param_SystemAppearanceIsDark,
        Param_SystemAppearanceIsSystemDark,
        Param_SystemAppearanceAreAppsDark,
        Param_ComCtl32Version,
        Param_GDIObjectCount,
        Param_UserObjectCount,
        Param_IsProcessDPIAware,
        Param_ProcessDPIAwareness,
        Param_ThreadDPIAwarenessContext,
        Param_ProcessSystemDPI,
        Param_WindowDPI,
        Param_WindowContentScaleFactor,
        Param_PathSeparator,
        Param_UserId,
        Param_UserName,
        Param_SystemEncodingName,
        Param_SystemLanguage,
#if wxCHECK_VERSION(3, 1, 6)
        Param_UILocaleName,
#endif
        Param_HostName,
        Param_FullHostName,
        Param_OSDescription,
        Param_OSVersion,
        Param_LinuxDistributionInfo,
        Param_OSDirectory,
        Param_CPUArchitectureName,
        Param_CPUCount,
        Param_IsPlatform64Bit,
        Param_IsPlatformLittleEndian,
    };

    std::shared_ptr<HostNameLookupState> m_hostNameLookupState;

    void OnObtainFullHostNameThread(wxThreadEvent& event);
    void StartObtainFullHostNameThread();
    void StopObtainFullHostNameThread();
};

#ifdef __WXMSW__

class MSWDPIAwarenessHelper
{
public:
    enum ProcessDPIAwareness
    {
        ProcessDPIUnaware         = 0,
        ProcessSystemDPIAware     = 1,
        ProcessPerMonitorDPIAware = 2,
        ProcessDPINotApplicable   = 33333, // returned on error/unsupported
    };

    enum DPIAwarenessContext
    {
        DPIAwarenessContextUnaware           = -1,
        DPIAwarenessContextSystemAware       = -2,
        DPIAwarenessContextPerMonitorAware   = -3,
        DPIAwarenessContextPerMonitorAwareV2 = -4,
        DPIAwarenessContextUnawareGdiscaled  = -5,
        DPIAwarenessContextNotApplicable     = -66666, // returned on error/unsupported
    };

    static bool IsThisProcessDPIAware();
    static ProcessDPIAwareness GetThisProcessDPIAwareness();
    static wxString GetThisProcessDPIAwarenessStr();

    // the two functions below return 0 if the information is not available
    static unsigned GetSystemDpiForThisProcess();
    static unsigned GetDpiForWindow(wxWindow* win);

    static DPIAwarenessContext GetThreadDPIAwarenessContext();
    static wxString GetThreadDPIAwarenessContextStr();

private:
    using MSW_DPI_AWARENESS_CONTEXT = HANDLE;

    using IsProcessDPIAware_t            = BOOL(WINAPI*)();
    using GetSystemDpiForProcess_t       = unsigned(WINAPI*)(HANDLE);
    using GetDpiForWindow_t              = unsigned(WINAPI*)(HWND);
    using GetThreadDpiAwarenessContext_t = MSW_DPI_AWARENESS_CONTEXT(WINAPI*)();
    using IsValidDpiAwarenessContext_t   = BOOL(WINAPI*)(MSW_DPI_AWARENESS_CONTEXT);
    using AreDpiAwarenessContextsEqual_t = BOOL(WINAPI*)(MSW_DPI_AWARENESS_CONTEXT, MSW_DPI_AWARENESS_CONTEXT);
    using GetProcessDpiAwareness_t       = HRESULT(WINAPI*)(HANDLE, DWORD*);

    // functions not available on all supported versions of Windows,
    // loaded dynamically just once
    struct Functions
    {
        IsProcessDPIAware_t            pfnIsProcessDPIAware{nullptr};
        GetSystemDpiForProcess_t       pfnGetSystemDpiForProcess{nullptr};
        GetDpiForWindow_t              pfnGetDpiForWindow{nullptr};
        GetThreadDpiAwarenessContext_t pfnGetThreadDpiAwarenessContext{nullptr};
        IsValidDpiAwarenessContext_t   pfnIsValidDpiAwarenessContext{nullptr};
        AreDpiAwarenessContextsEqual_t pfnAreDpiAwarenessContextsEqual{nullptr};
        GetProcessDpiAwareness_t       pfnGetProcessDpiAwareness{nullptr};

        wxDynamicLibrary dllShcore;

        Functions()
        {
            {
                wxLoadedDLL dllUser32("user32.dll");

                wxDL_INIT_FUNC(pfn, IsProcessDPIAware, dllUser32);
                wxDL_INIT_FUNC(pfn, GetSystemDpiForProcess, dllUser32);
                wxDL_INIT_FUNC(pfn, GetDpiForWindow, dllUser32);
                wxDL_INIT_FUNC(pfn, GetThreadDpiAwarenessContext, dllUser32);
                wxDL_INIT_FUNC(pfn, IsValidDpiAwarenessContext, dllUser32);
                wxDL_INIT_FUNC(pfn, AreDpiAwarenessContextsEqual, dllUser32);
            }

            if ( dllShcore.Load("shcore.dll", wxDL_VERBATIM | wxDL_QUIET) )
            {
                wxDL_INIT_FUNC(pfn, GetProcessDpiAwareness, dllShcore);

                if ( !pfnGetProcessDpiAwareness )
                    dllShcore.Unload();
            }
        }
    };

    static const Functions& GetFunctions()
    {
        static const Functions s_functions;

        return s_functions;
    }
};

bool MSWDPIAwarenessHelper::IsThisProcessDPIAware()
{
    const Functions& f = GetFunctions();

    return f.pfnIsProcessDPIAware && f.pfnIsProcessDPIAware() == TRUE;
}

MSWDPIAwarenessHelper::ProcessDPIAwareness MSWDPIAwarenessHelper::GetThisProcessDPIAwareness()
{
    const Functions& f = GetFunctions();

    if ( !f.pfnGetProcessDpiAwareness )
        return ProcessDPINotApplicable;

    DWORD value = ProcessDPINotApplicable;
    const HRESULT hr = f.pfnGetProcessDpiAwareness(nullptr, &value);

    if ( FAILED(hr) )
    {
        wxLogApiError("GetProcessDpiAwareness", hr);
        return ProcessDPINotApplicable;
    }

    switch ( value )
    {
        case ProcessDPIUnaware:
        case ProcessSystemDPIAware:
        case ProcessPerMonitorDPIAware:
            return static_cast<ProcessDPIAwareness>(value);
    }

    return ProcessDPINotApplicable;
}

wxString MSWDPIAwarenessHelper::GetThisProcessDPIAwarenessStr()
{
    const ProcessDPIAwareness awareness = GetThisProcessDPIAwareness();
    wxString result;

    switch ( awareness )
    {
        case ProcessDPIUnaware:
            result = _("DPI Unaware");
            break;
        case ProcessSystemDPIAware:
            result = _("System DPI Aware");
            break;
        case ProcessPerMonitorDPIAware:
            result = _("Per Monitor DPI Aware");
            break;
        default:
            result = _("<Not Applicable / Unknown>");
    }

    return result;
}

unsigned MSWDPIAwarenessHelper::GetSystemDpiForThisProcess()
{
    const Functions& f = GetFunctions();

    if ( !f.pfnGetSystemDpiForProcess )
        return 0;

    return f.pfnGetSystemDpiForProcess(::GetCurrentProcess());
}

unsigned MSWDPIAwarenessHelper::GetDpiForWindow(wxWindow* win)
{
    const Functions& f = GetFunctions();

    if ( !f.pfnGetDpiForWindow )
        return 0;

    return f.pfnGetDpiForWindow(win->GetHWND());
}

MSWDPIAwarenessHelper::DPIAwarenessContext MSWDPIAwarenessHelper::GetThreadDPIAwarenessContext()
{
    static const MSW_DPI_AWARENESS_CONTEXT MSW_DPI_AWARENESS_CONTEXT_UNAWARE              = (MSW_DPI_AWARENESS_CONTEXT)-1;
    static const MSW_DPI_AWARENESS_CONTEXT MSW_DPI_AWARENESS_CONTEXT_SYSTEM_AWARE         = (MSW_DPI_AWARENESS_CONTEXT)-2;
    static const MSW_DPI_AWARENESS_CONTEXT MSW_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE    = (MSW_DPI_AWARENESS_CONTEXT)-3;
    static const MSW_DPI_AWARENESS_CONTEXT MSW_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 = (MSW_DPI_AWARENESS_CONTEXT)-4;
    static const MSW_DPI_AWARENESS_CONTEXT MSW_DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED    = (MSW_DPI_AWARENESS_CONTEXT)-5;

    const Functions& f = GetFunctions();

    if ( !f.pfnGetThreadDpiAwarenessContext
         || !f.pfnAreDpiAwarenessContextsEqual
         || !f.pfnIsValidDpiAwarenessContext )
    {
        return DPIAwarenessContextNotApplicable;
    }

    const MSW_DPI_AWARENESS_CONTEXT value = f.pfnGetThreadDpiAwarenessContext();

    if ( !f.pfnIsValidDpiAwarenessContext(value) )
        return DPIAwarenessContextNotApplicable;

    if ( f.pfnAreDpiAwarenessContextsEqual(value, MSW_DPI_AWARENESS_CONTEXT_UNAWARE) )
        return DPIAwarenessContextUnaware;
    if ( f.pfnAreDpiAwarenessContextsEqual(value, MSW_DPI_AWARENESS_CONTEXT_SYSTEM_AWARE) )
        return DPIAwarenessContextSystemAware;
    if ( f.pfnAreDpiAwarenessContextsEqual(value, MSW_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE) )
        return DPIAwarenessContextPerMonitorAware;
    if ( f.pfnAreDpiAwarenessContextsEqual(value, MSW_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) )
        return DPIAwarenessContextPerMonitorAwareV2;
    if ( f.pfnAreDpiAwarenessContextsEqual(value, MSW_DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED) )
        return DPIAwarenessContextUnawareGdiscaled;

    return DPIAwarenessContextNotApplicable;
}

wxString MSWDPIAwarenessHelper::GetThreadDPIAwarenessContextStr()
{
    const DPIAwarenessContext context = GetThreadDPIAwarenessContext();
    wxString result;

    switch ( context )
    {
        case DPIAwarenessContextUnaware:
            result = _("DPI Unaware");
            break;
        case DPIAwarenessContextSystemAware:
            result = _("System DPI Aware");
            break;
        case DPIAwarenessContextPerMonitorAware:
            result = _("Per Monitor DPI Aware");
            break;
        case DPIAwarenessContextPerMonitorAwareV2:
            result = _("Per Monitor DPI Aware V2");
            break;
        case DPIAwarenessContextUnawareGdiscaled:
            result = _("DPI Unaware GDI Scaled");
            break;
        default:
            result = _("<Not Applicable / Unknown>");
    }

    return result;
}

#endif // #ifdef __WXMSW__

class ObtainFullHostNameThread : public wxThread
{
public:
    ObtainFullHostNameThread(const std::shared_ptr<HostNameLookupState>& state, unsigned requestId)
        : wxThread(wxTHREAD_DETACHED),
          m_state(state),
          m_requestId(requestId)
    {}

protected:
    std::shared_ptr<HostNameLookupState> m_state;
    const unsigned m_requestId;

    ExitCode Entry() override
    {
        const wxString hostName = wxGetFullHostName();

        // the view may have been destroyed or may have requested
        // a newer result in the meantime
        wxCriticalSectionLocker locker(m_state->cs);

        if ( m_state->sink && m_state->requestId == m_requestId )
        {
            wxThreadEvent* const evt = new wxThreadEvent();

            evt->SetString(hostName);
            wxQueueEvent(m_state->sink, evt);
        }

        return static_cast<wxThread::ExitCode>(nullptr);
    }
};


wxString GetThemeName()
{
#if defined(__WXMSW__)
    wxString name = _("<Unavailable>");
#elif defined(__WXGTK__)
    wxString name;
#else
    wxString name = _("<Unsupported on This Platform>");
#endif

#ifdef __WXMSW__
    static const int buffSize = 1024;

    WCHAR fileName[buffSize+1]{0};
    WCHAR colorName[buffSize+1]{0};
    WCHAR displayName[buffSize+1]{0};

    if ( SUCCEEDED(::GetCurrentThemeName(fileName, buffSize, colorName, buffSize, nullptr, 0)) )
    {
        if ( SUCCEEDED(::GetThemeDocumentationProperty(fileName, SZ_THDOCPROP_DISPLAYNAME, displayName, buffSize)) )
        {
            HIGHCONTRAST hc{};

            name.Printf("%s / %s", displayName, colorName);

            hc.cbSize = sizeof(hc);
            if ( ::SystemParametersInfo(SPI_GETHIGHCONTRAST, sizeof(hc), &hc, 0) )
            {
                if ( (hc.dwFlags & HCF_HIGHCONTRASTON) == HCF_HIGHCONTRASTON )
                    name += _(" (High Contrast)");
            }
        }
    }
#endif

#ifdef __WXGTK__
    GtkSettings* settings = gtk_settings_get_default();
    gchar* themeName = nullptr;

    g_object_get(settings, "gtk-theme-name", &themeName, nullptr);
    name = wxString::FromUTF8(themeName);
    g_free(themeName);
#endif

    return name;
}

#ifdef __WXMSW__
wxString DPIToString(unsigned dpi)
{
    return dpi ? wxString::Format("%u", dpi) : _("N/A");
}
#endif // #ifdef __WXMSW__

MiscellaneousView::~MiscellaneousView()
{
    StopObtainFullHostNameThread();
}

MiscellaneousView::MiscellaneousView(wxWindow* parent)
    : SysInfoListView(parent),
      m_hostNameLookupState(std::make_shared<HostNameLookupState>())
{
    m_hostNameLookupState->sink = this;

    InsertColumn(Column_Name, _("Name"));
    InsertColumn(Column_Value, _("Value"));

    AppendItemWithData(_("App Name"), Param_AppName);
    AppendItemWithData(_("App Display Name"), Param_AppDisplayName);
    AppendItemWithData(_("App Vendor Name"), Param_AppVendorName);
    AppendItemWithData(_("App Vendor Display Name"), Param_AppVendorDisplayName);
    AppendItemWithData(_("App Class Name"), Param_AppClassName);
    AppendItemWithData(_("App HasStderr"), Param_AppHasStderr);
    AppendItemWithData(_("64-bit Process"), Param_IsProcess64bit);
#ifdef __WXMSW__
    AppendItemWithData(_("Is <wx/wx.rc> Embedded"), Param_wxRCEmbedded);
    AppendItemWithData(_("Is Remote Session"), Param_IsRemoteSession);
#endif // #ifdef __WXMSW__
#ifdef __UNIX__
    AppendItemWithData(_("Unix Desktop Environment"), Param_UnixDesktopEnvironment);
#endif // #ifdef __UNIX__
    AppendItemWithData(_("Theme Name"), Param_ThemeName);
#if wxCHECK_VERSION(3, 1, 3)
    AppendItemWithData(_("System Appearance Name"), Param_SystemAppearanceName);
    AppendItemWithData(_("System Appearance IsDark"), Param_SystemAppearanceIsDark);
#endif

#if defined(__WXMSW__) && wxCHECK_VERSION(3, 3, 0)
    AppendItemWithData(_("System Appearance IsSystemDark"), Param_SystemAppearanceIsSystemDark);
    AppendItemWithData(_("System Appearance AreAppsDark"), Param_SystemAppearanceAreAppsDark);
#endif //#if defined(__WXMSW__) && wxCHECK_VERSION(3, 3, 0)

#ifdef __WXMSW__
    AppendItemWithData(_("ComCtl32.dll Version"), Param_ComCtl32Version);
    AppendItemWithData(_("GDI Object Count"), Param_GDIObjectCount);
    AppendItemWithData(_("User Object Count"), Param_UserObjectCount);
    AppendItemWithData(_("Is Process DPI Aware"), Param_IsProcessDPIAware);
    AppendItemWithData(_("Process DPI Awareness"), Param_ProcessDPIAwareness);
    AppendItemWithData(_("Thread DPI Awareness Context"), Param_ThreadDPIAwarenessContext);
    AppendItemWithData(_("System DPI for Process"), Param_ProcessSystemDPI);
    AppendItemWithData(_("DPI for This Window"), Param_WindowDPI);
#endif // #ifdef __WXMSW__

    AppendItemWithData(_("Window Content Scale Factor"), Param_WindowContentScaleFactor);
    AppendItemWithData(_("Path Separator"), Param_PathSeparator);
    AppendItemWithData(_("User Id"), Param_UserId);
    AppendItemWithData(_("User Name"), Param_UserName);
    AppendItemWithData(_("System Encoding"), Param_SystemEncodingName);
    AppendItemWithData(_("System Language"), Param_SystemLanguage);
#if wxCHECK_VERSION(3, 1, 6)
    AppendItemWithData(_("UI Locale Name"), Param_UILocaleName);
#endif
    AppendItemWithData(_("Host Name"), Param_HostName);
    AppendItemWithData(_("Full Host Name"), Param_FullHostName);
    AppendItemWithData(_("OS Description"), Param_OSDescription);
    AppendItemWithData(_("OS Version"), Param_OSVersion);
#ifdef __LINUX__
    AppendItemWithData(_("Linux Distribution Info"), Param_LinuxDistributionInfo);
#endif // #ifdef __LINUX__
    AppendItemWithData(_("OS Directory"), Param_OSDirectory);
#if wxCHECK_VERSION(3, 1, 5)
    AppendItemWithData(_("CPU Architecture Name"), Param_CPUArchitectureName);
#endif
    AppendItemWithData(_("64-bit Platform"), Param_IsPlatform64Bit);
    AppendItemWithData(_("CPU Count"), Param_CPUCount);
    AppendItemWithData(_("Little Endian"), Param_IsPlatformLittleEndian);

    Bind(wxEVT_THREAD, &MiscellaneousView::OnObtainFullHostNameThread, this);

    UpdateValues();
}

void MiscellaneousView::DoUpdateValues()
{
    int verMajor = 0, verMinor = 0, verMicro = 0;
    wxAppConsole* appInstance = wxAppConsole::GetInstance();
    wxAppTraits* appTraits = appInstance->GetTraits();
    const long itemCount = GetItemCount();
#ifdef __WXMSW__
    HANDLE hCurrentProcess = ::GetCurrentProcess();
    const DWORD GDIObjectCount = ::GetGuiResources(hCurrentProcess, GR_GDIOBJECTS);
    const DWORD UserObjectCount =  ::GetGuiResources(hCurrentProcess, GR_USEROBJECTS);
    bool wxRCEmbedded = false;
#endif
#ifdef __LINUX__
    const wxLinuxDistributionInfo linuxDistributionInfo = wxGetLinuxDistributionInfo();
#endif // #ifdef __LINUX__

#if wxCHECK_VERSION(3, 1, 3)
    const wxSystemAppearance systemAppearance = wxSystemSettings::GetAppearance();
#endif

#ifdef __WXMSW__
    {
        wxLogNull logNo;

        wxRCEmbedded = wxBitmap("wxBITMAP_STD_COLOURS").IsOk();
    }
#endif // #ifdef __WXMSW__

    wxGetOsVersion(&verMajor, &verMinor, &verMicro);

    StartObtainFullHostNameThread();

    for ( int i = 0; i < itemCount; ++i )
    {
        const long param = GetItemData(i);
        wxString value;

        switch ( param )
        {
            case Param_AppName:                   value = appInstance->GetAppName(); break;
            case Param_AppDisplayName:            value = appInstance->GetAppDisplayName(); break;
            case Param_AppVendorName:             value = appInstance->GetVendorName(); break;
            case Param_AppVendorDisplayName:      value = appInstance->GetVendorDisplayName(); break;
            case Param_AppClassName:              value = appInstance->GetClassName(); break;
            case Param_AppHasStderr:              value = appTraits->HasStderr() ? _("Yes") : _("No"); break;
            case Param_IsProcess64bit:            value = sizeof(void*) == 8 ? _("Yes") : _("No"); break;
#ifdef __WXMSW__
            case Param_wxRCEmbedded:              value = wxRCEmbedded ? _("Yes") : _("No"); break;
            case Param_IsRemoteSession:           value = ::GetSystemMetrics(SM_REMOTESESSION) != 0 ? _("Yes") : _("No"); break;
#endif // #ifdef __WXMSW__
#ifdef __UNIX__
            case Param_UnixDesktopEnvironment:    value = appTraits->GetDesktopEnvironment(); break;
#endif // #ifdef __UNIX__

            case Param_ThemeName:                 value = GetThemeName(); break;
#if wxCHECK_VERSION(3, 1, 3)
            case Param_SystemAppearanceName:      value = systemAppearance.GetName(); break;
            case Param_SystemAppearanceIsDark:    value = systemAppearance.IsDark() ? _("Yes") : _("No"); break;
#endif

#if defined(__WXMSW__) && wxCHECK_VERSION(3, 3, 0)
            case Param_SystemAppearanceIsSystemDark: value = systemAppearance.IsSystemDark() ? _("Yes") : _("No"); break;
            case Param_SystemAppearanceAreAppsDark:  value = systemAppearance.AreAppsDark() ? _("Yes") : _("No"); break;
#endif //#if defined(__WXMSW__) && wxCHECK_VERSION(3, 3, 0)

#ifdef __WXMSW__
            case Param_ComCtl32Version:           value.Printf("%d", wxApp::GetComCtl32Version()); break;
            case Param_GDIObjectCount:            value = GDIObjectCount ? wxString::Format("%lu", GDIObjectCount) : _("N/A"); break;
            case Param_UserObjectCount:           value = UserObjectCount ? wxString::Format("%lu", UserObjectCount) : _("N/A"); break;
            case Param_IsProcessDPIAware:         value = MSWDPIAwarenessHelper::IsThisProcessDPIAware() ? _("Yes") : _("No"); break;
            case Param_ProcessDPIAwareness:       value = MSWDPIAwarenessHelper::GetThisProcessDPIAwarenessStr(); break;
            case Param_ThreadDPIAwarenessContext: value = MSWDPIAwarenessHelper::GetThreadDPIAwarenessContextStr(); break;
            case Param_ProcessSystemDPI:          value = DPIToString(MSWDPIAwarenessHelper::GetSystemDpiForThisProcess()); break;
            case Param_WindowDPI:                 value = DPIToString(MSWDPIAwarenessHelper::GetDpiForWindow(this)); break;
#endif // #ifdef __WXMSW__

            case Param_WindowContentScaleFactor:  value.Printf("%.2f", GetContentScaleFactor()); break;
            case Param_PathSeparator:             value = wxString(wxFileName::GetPathSeparator()); break;
            case Param_UserId:                    value = wxGetUserId(); break;
            case Param_UserName:                  value = wxGetUserName(); break;
            case Param_SystemEncodingName:        value = wxLocale::GetSystemEncodingName(); break;
            case Param_SystemLanguage:            value =  wxLocale::GetLanguageName(wxLocale::GetSystemLanguage()); break;
#if wxCHECK_VERSION(3, 1, 6)
            case Param_UILocaleName:              value =  wxUILocale::GetCurrent().GetName(); break;
#endif
            case Param_HostName:                  value = wxGetHostName(); break;
            case Param_FullHostName:              value = _("<Evaluating...>"); break;
            case Param_OSDescription:             value =  wxGetOsDescription(); break;
            case Param_OSVersion:                 value.Printf("%d.%d.%d", verMajor, verMinor, verMicro); break;
#ifdef __LINUX__
            case Param_LinuxDistributionInfo:     value.Printf("%s (%s)", linuxDistributionInfo.Description, linuxDistributionInfo.CodeName); break;
#endif // #ifdef __LINUX__
            case Param_OSDirectory:               value = wxGetOSDirectory(); break;
#if wxCHECK_VERSION(3, 1, 5)
            case  Param_CPUArchitectureName:      value = wxGetCpuArchitectureName(); break;
#endif
            case Param_IsPlatform64Bit:           value = wxIsPlatform64Bit() ? _("Yes") : _("No"); break;
            case Param_CPUCount:                  value.Printf("%d", wxThread::GetCPUCount()); break;
            case Param_IsPlatformLittleEndian:    value =  wxIsPlatformLittleEndian() ? _("Yes") : _("No"); break;

            default:
                wxFAIL;
        }

        SetItem(i, Column_Value, value);
    }
}

void MiscellaneousView::OnObtainFullHostNameThread(wxThreadEvent& event)
{
    const long itemIndex = FindItem(-1, Param_FullHostName);

    if ( itemIndex != wxNOT_FOUND )
        SetItem(itemIndex, Column_Value, event.GetString());
}

void MiscellaneousView::StartObtainFullHostNameThread()
{
    unsigned requestId;

    {
        wxCriticalSectionLocker locker(m_hostNameLookupState->cs);

        // invalidates the result of any previous lookup that is still running
        requestId = ++m_hostNameLookupState->requestId;
    }

    ObtainFullHostNameThread* const thread = new ObtainFullHostNameThread(m_hostNameLookupState, requestId);

    // the thread is detached and deletes itself when finished
    if ( thread->Run() != wxTHREAD_NO_ERROR )
    {
        delete thread;
        wxLogError(_("Could not create the thread needed to obtain the full host name."));
    }
}

void MiscellaneousView::StopObtainFullHostNameThread()
{
    wxCriticalSectionLocker locker(m_hostNameLookupState->cs);

    m_hostNameLookupState->sink = nullptr;
}

/*************************************************

    PreprocessorDefinesView

*************************************************/

class PreprocessorDefinesView : public SysInfoListView
{
public:
    PreprocessorDefinesView(wxWindow* parent);

    wxArrayString GetValues(const wxString& separator) const override
    {
        return GetNameAndValueValues(Column_Name, Column_Value, separator);
    }

protected:
    void DoUpdateValues() override;
private:
    enum
    {
        Column_Name = 0,
        Column_Value,
    };

    void AppendDefineItem(const wxString& name, const wxString& value);
};

PreprocessorDefinesView::PreprocessorDefinesView(wxWindow* parent)
    : SysInfoListView(parent)
{
    InsertColumn(Column_Name, _("Name"));
    InsertColumn(Column_Value, _("Value"));

    UpdateValues();
}

wxString DefineValueToText(const wxString& name, const wxString& value)
{
    if ( name == value )
        return _("<Is Not Defined>");

    if ( value.empty() )
        return _("<Is Defined>");

    return value;
}

// braces instead of do { } while (0) so that the macro can be used without a trailing semicolon
#define APPEND_DEFINE_ITEM(value)     { AppendDefineItem(#value, DefineValueToText(#value, wxSTRINGIZE_T(value))); }

// Some macros (e.g. wxHAS_*) are defined as empty, which cannot be passed to
// APPEND_DEFINE_ITEM() with all compilers, so only their presence is checked,
// using hasDefine which must be set by the caller after testing the macro.
#define APPEND_HAS_DEFINE_ITEM(name) \
    { AppendDefineItem(name, hasDefine ? _("Yes") : _("No")); hasDefine = false; }

void PreprocessorDefinesView::AppendDefineItem(const wxString& name, const wxString& value)
{
    const long itemIndex = InsertItem(GetItemCount(), name);

    if ( itemIndex != -1 )
        SetItem(itemIndex, Column_Value, value);
}

void PreprocessorDefinesView::DoUpdateValues()
{
    // preprocessor defines cannot be
    // changed when the application is running
    if ( GetItemCount() > 0 )
        return;

    bool hasDefine{false};

    APPEND_DEFINE_ITEM(__cplusplus)

#ifdef _MSC_VER
    APPEND_DEFINE_ITEM(_MSC_VER)
    APPEND_DEFINE_ITEM(_MSC_FULL_VER)
    APPEND_DEFINE_ITEM(_MSVC_LANG)
    APPEND_DEFINE_ITEM(_DEBUG)
    APPEND_DEFINE_ITEM(_DLL)
    APPEND_DEFINE_ITEM(_MSVC_STL_VERSION)
#endif // #ifdef _MSC_VER

#ifdef __GNUC__
    APPEND_DEFINE_ITEM(__GNUC__)
    APPEND_DEFINE_ITEM(__GNUC_MINOR__)
    APPEND_DEFINE_ITEM(__GNUC_PATCHLEVEL__)
    APPEND_DEFINE_ITEM(__GLIBCXX__)
#endif // #ifdef(__GNUC__)

#ifdef __MINGW32__
    APPEND_DEFINE_ITEM(__MINGW32__)
    APPEND_DEFINE_ITEM(__MINGW32_MAJOR_VERSION)
    APPEND_DEFINE_ITEM(__MINGW32_MINOR_VERSION)
#endif

#ifdef __MINGW64__
    APPEND_DEFINE_ITEM(__MINGW64__)
    APPEND_DEFINE_ITEM(__MINGW64_VERSION_MAJOR)
    APPEND_DEFINE_ITEM(__MINGW64_VERSION_MINOR)
#endif

#ifdef __MINGW32_TOOLCHAIN__
    APPEND_DEFINE_ITEM(__MINGW32_TOOLCHAIN__)
#endif

#ifdef __MINGW64_TOOLCHAIN__
    APPEND_DEFINE_ITEM(__MINGW64_TOOLCHAIN__)
#endif

#ifdef __clang__
    APPEND_DEFINE_ITEM(__clang__)
    APPEND_DEFINE_ITEM(__clang_major__)
    APPEND_DEFINE_ITEM(__clang_minor__)
    APPEND_DEFINE_ITEM(__clang_patchlevel__)
    APPEND_DEFINE_ITEM(_LIBCPP_VERSION)
#endif

#ifdef __CYGWIN__
    APPEND_DEFINE_ITEM(__CYGWIN__)
#endif

    APPEND_DEFINE_ITEM(NDEBUG)

#ifdef __WXGTK__
    APPEND_DEFINE_ITEM(__WXGTK3__)
#endif

#ifdef __WXMSW__
    APPEND_DEFINE_ITEM(_UNICODE)
    APPEND_DEFINE_ITEM(WXUSINGDLL)
#endif

    AppendDefineItem("wxABI_VERSION", wxString::Format("%ld", (long)wxABI_VERSION));

#ifdef WXWIN_COMPATIBILITY_2_8
    APPEND_DEFINE_ITEM(WXWIN_COMPATIBILITY_2_8)
#endif
#ifdef WXWIN_COMPATIBILITY_3_0
    APPEND_DEFINE_ITEM(WXWIN_COMPATIBILITY_3_0)
#endif
#ifdef WXWIN_COMPATIBILITY_3_2
    APPEND_DEFINE_ITEM(WXWIN_COMPATIBILITY_3_2)
#endif

    AppendDefineItem("WX_BUILD_OPTIONS_SIGNATURE", WX_BUILD_OPTIONS_SIGNATURE);

    APPEND_DEFINE_ITEM(wxUSE_REPRODUCIBLE_BUILD)
    APPEND_DEFINE_ITEM(wxDEBUG_LEVEL)
    APPEND_DEFINE_ITEM(wxUSE_ON_FATAL_EXCEPTION)
    APPEND_DEFINE_ITEM(wxUSE_STACKWALKER)
    APPEND_DEFINE_ITEM(wxUSE_DEBUGREPORT)
    APPEND_DEFINE_ITEM(wxUSE_DEBUG_CONTEXT)
    APPEND_DEFINE_ITEM(wxUSE_MEMORY_TRACING)
    APPEND_DEFINE_ITEM(wxUSE_GLOBAL_MEMORY_OPERATORS)
    APPEND_DEFINE_ITEM(wxUSE_DEBUG_NEW_ALWAYS)
#ifdef _MSC_VER
    APPEND_DEFINE_ITEM(wxUSE_VC_CRTDBG)
#endif // #ifdef _MSC_VER
#ifdef __WXMSW__
    APPEND_DEFINE_ITEM(wxUSE_NO_MANIFEST)
    #if !defined(wxUSE_NO_MANIFEST) || (wxUSE_NO_MANIFEST == 0)
        APPEND_DEFINE_ITEM(wxUSE_RC_MANIFEST)
    #endif //#if !defined(wxUSE_NO_MANIFEST) || (wxUSE_NO_MANIFEST == 0)
    #if defined(wxUSE_RC_MANIFEST) && wxUSE_RC_MANIFEST
        APPEND_DEFINE_ITEM(wxUSE_DPI_AWARE_MANIFEST)
    #endif // #if defined(wxUSE_RC_MANIFEST) && wxUSE_RC_MANIFEST
#endif // #ifdef __WXMSW__
    APPEND_DEFINE_ITEM(wxUSE_UNICODE)
    APPEND_DEFINE_ITEM(wxUSE_UNICODE_WCHAR)
    APPEND_DEFINE_ITEM(wxUSE_UNICODE_UTF8)
    APPEND_DEFINE_ITEM(wxUSE_UTF8_LOCALE_ONLY)
    APPEND_DEFINE_ITEM(wxUSE_UNSAFE_WXSTRING_CONV)

// this may or may be not defined depending on wxUSE_UNSAFE_WXSTRING_CONV definition
// and whether the user defined it in their project
#ifdef wxNO_UNSAFE_WXSTRING_CONV
    hasDefine = true;
#endif
    APPEND_HAS_DEFINE_ITEM("wxNO_UNSAFE_WXSTRING_CONV")

    APPEND_DEFINE_ITEM(wxNO_IMPLICIT_WXSTRING_ENCODING)
    APPEND_DEFINE_ITEM(wxUSE_EXCEPTIONS)
    APPEND_DEFINE_ITEM(wxUSE_EXTENDED_RTTI)
    APPEND_DEFINE_ITEM(wxUSE_LOG)
    APPEND_DEFINE_ITEM(wxUSE_LOGWINDOW)
    APPEND_DEFINE_ITEM(wxUSE_LOGGUI)
    APPEND_DEFINE_ITEM(wxUSE_LOG_DIALOG)
    APPEND_DEFINE_ITEM(wxUSE_CMDLINE_PARSER)
    APPEND_DEFINE_ITEM(wxUSE_THREADS)
    APPEND_DEFINE_ITEM(wxUSE_STREAMS)
    APPEND_DEFINE_ITEM(wxUSE_PRINTF_POS_PARAMS)
    APPEND_DEFINE_ITEM(wxUSE_COMPILER_TLS)
#if (!wxCHECK_VERSION(3,3,0))
    APPEND_DEFINE_ITEM(wxUSE_STL)
    APPEND_DEFINE_ITEM(wxUSE_STD_DEFAULT)
    APPEND_DEFINE_ITEM(wxUSE_STD_CONTAINERS_COMPATIBLY)
    APPEND_DEFINE_ITEM(wxUSE_STD_STRING)
#endif // #if (!wxCHECK_VERSION(3,3,0)
    APPEND_DEFINE_ITEM(wxUSE_STD_CONTAINERS)
    APPEND_DEFINE_ITEM(wxUSE_STD_IOSTREAM)
    APPEND_DEFINE_ITEM(wxUSE_STD_STRING_CONV_IN_WXSTRING)
    APPEND_DEFINE_ITEM(wxUSE_LONGLONG)
    APPEND_DEFINE_ITEM(wxUSE_BASE64)
    APPEND_DEFINE_ITEM(wxUSE_CONSOLE_EVENTLOOP)
    APPEND_DEFINE_ITEM(wxUSE_FILE)
    APPEND_DEFINE_ITEM(wxUSE_FFILE)
    APPEND_DEFINE_ITEM(wxUSE_FSVOLUME)
    APPEND_DEFINE_ITEM(wxUSE_SECRETSTORE)
    APPEND_DEFINE_ITEM(wxUSE_STDPATHS)
    APPEND_DEFINE_ITEM(wxUSE_TEXTBUFFER)
    APPEND_DEFINE_ITEM(wxUSE_TEXTFILE)
    APPEND_DEFINE_ITEM(wxUSE_INTL)
    APPEND_DEFINE_ITEM(wxUSE_XLOCALE)
    APPEND_DEFINE_ITEM(wxUSE_DATETIME)
    APPEND_DEFINE_ITEM(wxUSE_TIMER)
    APPEND_DEFINE_ITEM(wxUSE_STOPWATCH)
    APPEND_DEFINE_ITEM(wxUSE_FSWATCHER)
    APPEND_DEFINE_ITEM(wxUSE_CONFIG)
    APPEND_DEFINE_ITEM(wxUSE_CONFIG_NATIVE)
    APPEND_DEFINE_ITEM(wxUSE_DIALUP_MANAGER)
    APPEND_DEFINE_ITEM(wxUSE_DYNLIB_CLASS)
    APPEND_DEFINE_ITEM(wxUSE_DYNAMIC_LOADER)
    APPEND_DEFINE_ITEM(wxUSE_SOCKETS)
    APPEND_DEFINE_ITEM(wxUSE_IPV6)
    APPEND_DEFINE_ITEM(wxUSE_FILESYSTEM)
    APPEND_DEFINE_ITEM(wxUSE_FS_ZIP)
    APPEND_DEFINE_ITEM(wxUSE_FS_ARCHIVE)
    APPEND_DEFINE_ITEM(wxUSE_FS_INET)
    APPEND_DEFINE_ITEM(wxUSE_ARCHIVE_STREAMS)
    APPEND_DEFINE_ITEM(wxUSE_ZIPSTREAM)
    APPEND_DEFINE_ITEM(wxUSE_TARSTREAM)
    APPEND_DEFINE_ITEM(wxUSE_ZLIB)
    APPEND_DEFINE_ITEM(wxUSE_LIBLZMA)
    APPEND_DEFINE_ITEM(wxUSE_APPLE_IEEE)
    APPEND_DEFINE_ITEM(wxUSE_JOYSTICK)
    APPEND_DEFINE_ITEM(wxUSE_FONTENUM)
    APPEND_DEFINE_ITEM(wxUSE_FONTMAP)
    APPEND_DEFINE_ITEM(wxUSE_MIMETYPE)
    APPEND_DEFINE_ITEM(wxUSE_PROTOCOL)
    APPEND_DEFINE_ITEM(wxUSE_PROTOCOL_FILE)
    APPEND_DEFINE_ITEM(wxUSE_PROTOCOL_FTP)
    APPEND_DEFINE_ITEM(wxUSE_PROTOCOL_HTTP)
    APPEND_DEFINE_ITEM(wxUSE_URL)
    APPEND_DEFINE_ITEM(wxUSE_URL_NATIVE)
    APPEND_DEFINE_ITEM(wxUSE_VARIANT)
    APPEND_DEFINE_ITEM(wxUSE_ANY)
    APPEND_DEFINE_ITEM(wxUSE_REGEX)
    APPEND_DEFINE_ITEM(wxUSE_SYSTEM_OPTIONS)
    APPEND_DEFINE_ITEM(wxUSE_SOUND)
    APPEND_DEFINE_ITEM(wxUSE_MEDIACTRL)
    APPEND_DEFINE_ITEM(wxUSE_XRC)
    APPEND_DEFINE_ITEM(wxUSE_XML)
    APPEND_DEFINE_ITEM(wxUSE_AUI)
    APPEND_DEFINE_ITEM(wxUSE_RIBBON)
    APPEND_DEFINE_ITEM(wxUSE_PROPGRID)
    APPEND_DEFINE_ITEM(wxUSE_STC)
    APPEND_DEFINE_ITEM(wxUSE_WEBVIEW)
#ifdef __WXMSW__
    APPEND_DEFINE_ITEM(wxUSE_WEBVIEW_IE)
    #ifdef wxUSE_WEBVIEW_EDGE
        APPEND_DEFINE_ITEM(wxUSE_WEBVIEW_EDGE)
    #endif
    #ifdef wxUSE_WEBVIEW_EDGE_STATIC
        APPEND_DEFINE_ITEM(wxUSE_WEBVIEW_EDGE_STATIC)
    #endif
#else
    APPEND_DEFINE_ITEM(wxUSE_WEBVIEW_WEBKIT)
#endif // #else
#ifdef __WXGTK__
    APPEND_DEFINE_ITEM(wxUSE_WEBVIEW_WEBKIT2)
#endif // #ifdef __WXGTK__

    APPEND_DEFINE_ITEM(wxUSE_GRAPHICS_CONTEXT)
    APPEND_DEFINE_ITEM(wxUSE_CAIRO)
    APPEND_DEFINE_ITEM(wxUSE_CONTROLS)
    APPEND_DEFINE_ITEM(wxUSE_MARKUP)
    APPEND_DEFINE_ITEM(wxUSE_POPUPWIN)
    APPEND_DEFINE_ITEM(wxUSE_TIPWINDOW)
    APPEND_DEFINE_ITEM(wxUSE_ACTIVITYINDICATOR)
    APPEND_DEFINE_ITEM(wxUSE_ANIMATIONCTRL)
    APPEND_DEFINE_ITEM(wxUSE_BANNERWINDOW)
    APPEND_DEFINE_ITEM(wxUSE_BUTTON)
    APPEND_DEFINE_ITEM(wxUSE_BMPBUTTON)
    APPEND_DEFINE_ITEM(wxUSE_CALENDARCTRL)
    APPEND_DEFINE_ITEM(wxUSE_CHECKBOX)
    APPEND_DEFINE_ITEM(wxUSE_CHOICE)
    APPEND_DEFINE_ITEM(wxUSE_COLLPANE)
    APPEND_DEFINE_ITEM(wxUSE_COLOURPICKERCTRL)
    APPEND_DEFINE_ITEM(wxUSE_COMBOBOX)
    APPEND_DEFINE_ITEM(wxUSE_COMMANDLINKBUTTON)
    APPEND_DEFINE_ITEM(wxUSE_DATAVIEWCTRL)
    APPEND_DEFINE_ITEM(wxUSE_DATEPICKCTRL)
    APPEND_DEFINE_ITEM(wxUSE_DIRPICKERCTRL)
    APPEND_DEFINE_ITEM(wxUSE_EDITABLELISTBOX)
    APPEND_DEFINE_ITEM(wxUSE_FILECTRL)
    APPEND_DEFINE_ITEM(wxUSE_FILEPICKERCTRL)
    APPEND_DEFINE_ITEM(wxUSE_FONTPICKERCTRL)
    APPEND_DEFINE_ITEM(wxUSE_GAUGE)
    APPEND_DEFINE_ITEM(wxUSE_HEADERCTRL)
    APPEND_DEFINE_ITEM(wxUSE_HYPERLINKCTRL)
    APPEND_DEFINE_ITEM(wxUSE_LISTBOX)
    APPEND_DEFINE_ITEM(wxUSE_LISTCTRL)
    APPEND_DEFINE_ITEM(wxUSE_RADIOBOX)
    APPEND_DEFINE_ITEM(wxUSE_RADIOBTN)
    APPEND_DEFINE_ITEM(wxUSE_RICHMSGDLG)
    APPEND_DEFINE_ITEM(wxUSE_SCROLLBAR)
    APPEND_DEFINE_ITEM(wxUSE_SEARCHCTRL)
    APPEND_DEFINE_ITEM(wxUSE_SLIDER)
    APPEND_DEFINE_ITEM(wxUSE_SPINBTN)
    APPEND_DEFINE_ITEM(wxUSE_SPINCTRL)
    APPEND_DEFINE_ITEM(wxUSE_STATBOX)
    APPEND_DEFINE_ITEM(wxUSE_STATLINE)
    APPEND_DEFINE_ITEM(wxUSE_STATTEXT)
    APPEND_DEFINE_ITEM(wxUSE_STATBMP)
    APPEND_DEFINE_ITEM(wxUSE_TEXTCTRL)
    APPEND_DEFINE_ITEM(wxUSE_TIMEPICKCTRL)
    APPEND_DEFINE_ITEM(wxUSE_TOGGLEBTN)
    APPEND_DEFINE_ITEM(wxUSE_TREECTRL)
    APPEND_DEFINE_ITEM(wxUSE_TREELISTCTRL)
    APPEND_DEFINE_ITEM(wxUSE_STATUSBAR)
    APPEND_DEFINE_ITEM(wxUSE_NATIVE_STATUSBAR)
    APPEND_DEFINE_ITEM(wxUSE_TOOLBAR)
    APPEND_DEFINE_ITEM(wxUSE_TOOLBAR_NATIVE)
    APPEND_DEFINE_ITEM(wxUSE_NOTEBOOK)
    APPEND_DEFINE_ITEM(wxUSE_LISTBOOK)
    APPEND_DEFINE_ITEM(wxUSE_CHOICEBOOK)
    APPEND_DEFINE_ITEM(wxUSE_TREEBOOK)
    APPEND_DEFINE_ITEM(wxUSE_TOOLBOOK)
    APPEND_DEFINE_ITEM(wxUSE_TASKBARICON)
    APPEND_DEFINE_ITEM(wxUSE_GRID)
    APPEND_DEFINE_ITEM(wxUSE_MINIFRAME)
    APPEND_DEFINE_ITEM(wxUSE_COMBOCTRL)
    APPEND_DEFINE_ITEM(wxUSE_ODCOMBOBOX)
    APPEND_DEFINE_ITEM(wxUSE_BITMAPCOMBOBOX)
    APPEND_DEFINE_ITEM(wxUSE_REARRANGECTRL)
    APPEND_DEFINE_ITEM(wxUSE_ADDREMOVECTRL)
    APPEND_DEFINE_ITEM(wxUSE_ACCEL)
    APPEND_DEFINE_ITEM(wxUSE_ARTPROVIDER_STD)
    APPEND_DEFINE_ITEM(wxUSE_ARTPROVIDER_TANGO)
    APPEND_DEFINE_ITEM(wxUSE_CARET)
    APPEND_DEFINE_ITEM(wxUSE_DISPLAY)
    APPEND_DEFINE_ITEM(wxUSE_GEOMETRY)
    APPEND_DEFINE_ITEM(wxUSE_IMAGLIST)
    APPEND_DEFINE_ITEM(wxUSE_INFOBAR)
    APPEND_DEFINE_ITEM(wxUSE_MENUS)
    APPEND_DEFINE_ITEM(wxUSE_NOTIFICATION_MESSAGE)
    APPEND_DEFINE_ITEM(wxUSE_PREFERENCES_EDITOR)
    APPEND_DEFINE_ITEM(wxUSE_PRIVATE_FONTS)
    APPEND_DEFINE_ITEM(wxUSE_RICHTOOLTIP)
    APPEND_DEFINE_ITEM(wxUSE_SASH)
    APPEND_DEFINE_ITEM(wxUSE_SPLITTER)
    APPEND_DEFINE_ITEM(wxUSE_TOOLTIPS)
    APPEND_DEFINE_ITEM(wxUSE_VALIDATORS)
    APPEND_DEFINE_ITEM(wxUSE_AUTOID_MANAGEMENT)
    APPEND_DEFINE_ITEM(wxUSE_BUSYINFO)
    APPEND_DEFINE_ITEM(wxUSE_CHOICEDLG)
    APPEND_DEFINE_ITEM(wxUSE_COLOURDLG)
    APPEND_DEFINE_ITEM(wxUSE_DIRDLG)
    APPEND_DEFINE_ITEM(wxUSE_FILEDLG)
    APPEND_DEFINE_ITEM(wxUSE_FINDREPLDLG)
    APPEND_DEFINE_ITEM(wxUSE_FONTDLG)
    APPEND_DEFINE_ITEM(wxUSE_MSGDLG)
    APPEND_DEFINE_ITEM(wxUSE_PROGRESSDLG)
    APPEND_DEFINE_ITEM(wxUSE_NATIVE_PROGRESSDLG)
    APPEND_DEFINE_ITEM(wxUSE_STARTUP_TIPS)
    APPEND_DEFINE_ITEM(wxUSE_TEXTDLG)
    APPEND_DEFINE_ITEM(wxUSE_NUMBERDLG)
    APPEND_DEFINE_ITEM(wxUSE_SPLASH)
    APPEND_DEFINE_ITEM(wxUSE_WIZARDDLG)
    APPEND_DEFINE_ITEM(wxUSE_ABOUTDLG)
    APPEND_DEFINE_ITEM(wxUSE_FILE_HISTORY)
    APPEND_DEFINE_ITEM(wxUSE_METAFILE)
    APPEND_DEFINE_ITEM(wxUSE_WIN_METAFILES_ALWAYS)
    APPEND_DEFINE_ITEM(wxUSE_MDI)
    APPEND_DEFINE_ITEM(wxUSE_DOC_VIEW_ARCHITECTURE)
    APPEND_DEFINE_ITEM(wxUSE_MDI_ARCHITECTURE)
    APPEND_DEFINE_ITEM(wxUSE_PRINTING_ARCHITECTURE)
    APPEND_DEFINE_ITEM(wxUSE_HTML)
    APPEND_DEFINE_ITEM(wxUSE_GLCANVAS)
    APPEND_DEFINE_ITEM(wxUSE_RICHTEXT)
    APPEND_DEFINE_ITEM(wxUSE_CLIPBOARD)
    APPEND_DEFINE_ITEM(wxUSE_DATAOBJ)
    APPEND_DEFINE_ITEM(wxUSE_DRAG_AND_DROP)
    APPEND_DEFINE_ITEM(wxUSE_SNGLINST_CHECKER)
    APPEND_DEFINE_ITEM(wxUSE_DRAGIMAGE)
    APPEND_DEFINE_ITEM(wxUSE_IPC)
    APPEND_DEFINE_ITEM(wxUSE_HELP)
    APPEND_DEFINE_ITEM(wxUSE_WXHTML_HELP)
    APPEND_DEFINE_ITEM(wxUSE_CONSTRAINTS)
    APPEND_DEFINE_ITEM(wxUSE_SPLINES)
    APPEND_DEFINE_ITEM(wxUSE_MOUSEWHEEL)
    APPEND_DEFINE_ITEM(wxUSE_UIACTIONSIMULATOR)
    APPEND_DEFINE_ITEM(wxUSE_POSTSCRIPT)
    APPEND_DEFINE_ITEM(wxUSE_AFM_FOR_POSTSCRIPT)
    APPEND_DEFINE_ITEM(wxUSE_SVG)
    APPEND_DEFINE_ITEM(wxUSE_DC_TRANSFORM_MATRIX)
    APPEND_DEFINE_ITEM(wxUSE_IMAGE)
    APPEND_DEFINE_ITEM(wxUSE_LIBPNG)
    APPEND_DEFINE_ITEM(wxUSE_LIBJPEG)
    APPEND_DEFINE_ITEM(wxUSE_LIBTIFF)
    APPEND_DEFINE_ITEM(wxUSE_TGA)
    APPEND_DEFINE_ITEM(wxUSE_GIF)
    APPEND_DEFINE_ITEM(wxUSE_PNM)
    APPEND_DEFINE_ITEM(wxUSE_PCX)
    APPEND_DEFINE_ITEM(wxUSE_IFF)
    APPEND_DEFINE_ITEM(wxUSE_XPM)
    APPEND_DEFINE_ITEM(wxUSE_ICO_CUR)
    APPEND_DEFINE_ITEM(wxUSE_PALETTE)
    APPEND_DEFINE_ITEM(wxUSE_WXDIB)
    APPEND_DEFINE_ITEM(wxUSE_OWNER_DRAWN)
    APPEND_DEFINE_ITEM(wxUSE_TASKBARICON_BALLOONS)
    APPEND_DEFINE_ITEM(wxUSE_TASKBARBUTTON)
    APPEND_DEFINE_ITEM(wxUSE_INICONF)
    APPEND_DEFINE_ITEM(wxUSE_DATEPICKCTRL_GENERIC)
    APPEND_DEFINE_ITEM(wxUSE_TIMEPICKCTRL_GENERIC)
#ifdef __WXMSW__
    APPEND_DEFINE_ITEM(wxUSE_ACCESSIBILITY)
    APPEND_DEFINE_ITEM(wxUSE_ACTIVEX)
    APPEND_DEFINE_ITEM(wxUSE_COMBOCTRL_POPUP_ANIMATION)
    APPEND_DEFINE_ITEM(wxUSE_COMMON_DIALOGS)
    APPEND_DEFINE_ITEM(wxUSE_CRASHREPORT)
    APPEND_DEFINE_ITEM(wxUSE_DBGHELP)
    APPEND_DEFINE_ITEM(wxUSE_DC_CACHEING)
    APPEND_DEFINE_ITEM(wxUSE_DDE_FOR_IPC)
    APPEND_DEFINE_ITEM(wxDIALOG_UNIT_COMPATIBILITY)
    APPEND_DEFINE_ITEM(wxUSE_ENH_METAFILE)
    APPEND_DEFINE_ITEM(wxUSE_GRAPHICS_GDIPLUS)
    APPEND_DEFINE_ITEM(wxUSE_GRAPHICS_DIRECT2D)
#endif // #ifdef __WXMSW__
    APPEND_DEFINE_ITEM(wxUSE_HOTKEY)

#ifdef __WXMSW__
    APPEND_DEFINE_ITEM(wxUSE_INKEDIT)
    APPEND_DEFINE_ITEM(wxUSE_MS_HTML_HELP)
    APPEND_DEFINE_ITEM(wxUSE_OLE)
    APPEND_DEFINE_ITEM(wxUSE_OLE_AUTOMATION)
    APPEND_DEFINE_ITEM(wxUSE_REGKEY)
    APPEND_DEFINE_ITEM(wxUSE_RICHEDIT)
    APPEND_DEFINE_ITEM(wxUSE_RICHEDIT2)
    APPEND_DEFINE_ITEM(wxUSE_UXTHEME)
    APPEND_DEFINE_ITEM(wxUSE_WINRT)
    APPEND_DEFINE_ITEM(wxUSE_WINSOCK2)
#endif // #ifdef __WXMSW__

#ifdef __WXGTK__
    APPEND_DEFINE_ITEM(wxUSE_DETECT_SM)
    APPEND_DEFINE_ITEM(wxUSE_GTKPRINT)
#endif // #ifdef __WXGTK__

#ifdef __UNIX__
    APPEND_DEFINE_ITEM(wxUSE_EPOLL_DISPATCHER)
    APPEND_DEFINE_ITEM(wxUSE_SELECT_DISPATCHER)
    APPEND_DEFINE_ITEM(wxUSE_GSTREAMER)
    APPEND_DEFINE_ITEM(wxUSE_LIBMSPACK)
    APPEND_DEFINE_ITEM(wxUSE_LIBSDL)
    APPEND_DEFINE_ITEM(wxUSE_PLUGINS)
    APPEND_DEFINE_ITEM(wxUSE_XTEST)
#endif // #ifdef __UNIX__

#ifdef __WXMAC__
    APPEND_DEFINE_ITEM(wxUSE_WEBKIT)
#endif// #ifdef __WXMAC__

#ifdef __WXUNIVERSAL__
    APPEND_DEFINE_ITEM(wxUSE_ALL_THEMES)
    APPEND_DEFINE_ITEM(wxUSE_THEME_GTK)
    APPEND_DEFINE_ITEM(wxUSE_THEME_METAL)
    APPEND_DEFINE_ITEM(wxUSE_THEME_MONO)
    APPEND_DEFINE_ITEM(wxUSE_THEME_WIN32)
#endif // #ifdef __WXUNIVERSAL__


#ifdef wxHAS_3STATE_CHECKBOX
    hasDefine = true;
#endif
    APPEND_HAS_DEFINE_ITEM("wxHAS_3STATE_CHECKBOX")

#ifdef wxHAS_ATOMIC_OPS
    hasDefine = true;
#endif
    APPEND_HAS_DEFINE_ITEM("wxHAS_ATOMIC_OPS")

#ifdef wxHAS_BITMAPTOGGLEBUTTON
    hasDefine = true;
#endif
    APPEND_HAS_DEFINE_ITEM("wxHAS_BITMAPTOGGLEBUTTON")

#ifdef wxHAS_MEMBER_DEFAULT
    hasDefine = true;
#endif
    APPEND_HAS_DEFINE_ITEM("wxHAS_MEMBER_DEFAULT")

#ifdef wxHAS_LARGE_FILES
    hasDefine = true;
#endif
    APPEND_HAS_DEFINE_ITEM("wxHAS_LARGE_FILES")

#ifdef wxHAS_LARGE_FFILES
    hasDefine = true;
#endif
    APPEND_HAS_DEFINE_ITEM("wxHAS_LARGE_FFILES")

#ifdef wxHAS_LONG_LONG_T_DIFFERENT_FROM_LONG
    hasDefine = true;
#endif
    APPEND_HAS_DEFINE_ITEM("wxHAS_LONG_LONG_T_DIFFERENT_FROM_LONG")

#ifdef wxHAS_MULTIPLE_FILEDLG_FILTERS
    hasDefine = true;
#endif
    APPEND_HAS_DEFINE_ITEM("wxHAS_MULTIPLE_FILEDLG_FILTERS")

#ifdef wxHAS_NATIVE_ANIMATIONCTRL
    hasDefine = true;
#endif
    APPEND_HAS_DEFINE_ITEM("wxHAS_NATIVE_ANIMATIONCTRL")

#ifdef wxHAS_NATIVE_DATAVIEWCTRL
    hasDefine = true;
#endif
    APPEND_HAS_DEFINE_ITEM("wxHAS_NATIVE_DATAVIEWCTRL")

#ifdef wxHAS_NATIVE_WINDOW
    hasDefine = true;
#endif
    APPEND_HAS_DEFINE_ITEM("wxHAS_NATIVE_WINDOW")

#ifdef wxHAS_NOEXCEPT
    hasDefine = true;
#endif
    APPEND_HAS_DEFINE_ITEM("wxHAS_NOEXCEPT")

#ifdef wxHAS_NULLPTR_T
    hasDefine = true;
#endif
    APPEND_HAS_DEFINE_ITEM("wxHAS_NULLPTR_T")

#ifdef wxHAS_IMAGES_IN_RESOURCES
    hasDefine = true;
#endif
    APPEND_HAS_DEFINE_ITEM("wxHAS_IMAGES_IN_RESOURCES")

#ifdef wxHAS_POWER_EVENTS
    hasDefine = true;
#endif
    APPEND_HAS_DEFINE_ITEM("wxHAS_POWER_EVENTS")

#ifdef wxHAS_RADIO_MENU_ITEMS
    hasDefine = true;
#endif
    APPEND_HAS_DEFINE_ITEM("wxHAS_RADIO_MENU_ITEMS")

#ifdef wxHAS_RAW_BITMAP
    hasDefine = true;
#endif
    APPEND_HAS_DEFINE_ITEM("wxHAS_RAW_BITMAP")

#ifdef wxHAS_RAW_KEY_CODES
    hasDefine = true;
#endif
    APPEND_HAS_DEFINE_ITEM("wxHAS_RAW_KEY_CODES")

#ifdef wxHAS_REGEX_ADVANCED
    hasDefine = true;
#endif
    APPEND_HAS_DEFINE_ITEM("wxHAS_REGEX_ADVANCED")

#ifdef wxHAS_TASK_BAR_ICON
    hasDefine = true;
#endif
    APPEND_HAS_DEFINE_ITEM("wxHAS_TASK_BAR_ICON")

#ifdef wxHAS_WINDOW_LABEL_IN_STATIC_BOX
    hasDefine = true;
#endif
    APPEND_HAS_DEFINE_ITEM("wxHAS_WINDOW_LABEL_IN_STATIC_BOX")

#ifdef wxHAS_MODE_T
    hasDefine = true;
#endif
    APPEND_HAS_DEFINE_ITEM("wxHAS_MODE_T")
}

} // anonymous namespace for helper classes


/*************************************************

    wxSystemInformationFrame

*************************************************/

wxSystemInformationFrame::wxSystemInformationFrame(wxWindow *parent, wxWindowID id, const wxString &title,
                                                   const wxPoint& pos, const wxSize& size,
                                                   long frameStyle, long createFlags)
{
    Create(parent, id, title, pos, size, frameStyle, createFlags);
}

wxSystemInformationFrame::wxSystemInformationFrame(wxWindow* parent, const wxSize& size, long createFlags)
{
#if wxCHECK_VERSION(3, 1, 0)
    // the default size is specified in DIPs
    const wxSize frameSize = wxWindow::FromDIP(size, parent);
#else
    const wxSize& frameSize = size;
#endif

    Create(parent, wxID_ANY, "wxSystemInformationFrame", wxDefaultPosition, frameSize, wxDEFAULT_FRAME_STYLE, createFlags);
}


bool wxSystemInformationFrame::Create(wxWindow *parent, wxWindowID id, const wxString &title,
                                      const wxPoint& pos, const wxSize& size,
                                      long frameStyle, long createFlags)
{
    if ( !wxFrame::Create(parent, id, title, pos, size, frameStyle) )
        return false;

    m_autoRefresh = (createFlags & AutoRefresh) != 0;

    // set up the timer and the event handlers first so that they are ready
    // for any messages sent while the rest of the frame is being created
    m_valuesUpdateTimer.SetOwner(this);
    Bind(wxEVT_TIMER, &wxSystemInformationFrame::OnUpdateValuesTimer, this);

    Bind(wxEVT_SYS_COLOUR_CHANGED, &wxSystemInformationFrame::OnSysColourChanged, this);
    Bind(wxEVT_DISPLAY_CHANGED, &wxSystemInformationFrame::OnDisplayChanged, this);

#if wxCHECK_VERSION(3, 1, 3)
    Bind(wxEVT_DPI_CHANGED, &wxSystemInformationFrame::OnDPIChanged, this);
#endif

    wxPanel* mainPanel = new wxPanel(this);
    wxBoxSizer* mainPanelSizer = new wxBoxSizer(wxVERTICAL);

    // add buttons

    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);

    wxButton* refreshButton = new wxButton(mainPanel, wxID_ANY, _("Refresh"));
    refreshButton->Bind(wxEVT_BUTTON, &wxSystemInformationFrame::OnRefresh, this);
    buttonSizer->Add(refreshButton, wxSizerFlags().Border(wxRIGHT));

    wxButton* detailsButton = nullptr;
    if ( createFlags & (ViewSystemColours | ViewSystemFonts) )
    {
        detailsButton = new wxButton(mainPanel, wxID_ANY, _("wxSYS Colour or Font Details..."));
        detailsButton->Bind(wxEVT_BUTTON, &wxSystemInformationFrame::OnShowDetailedInformation, this);
        buttonSizer->Add(detailsButton, wxSizerFlags().Border(wxRIGHT));
    }

    wxButton* wxInfoButton = new wxButton(mainPanel, wxID_ANY, _("wxInfoMessageBox..."));
    wxInfoButton->Bind(wxEVT_BUTTON, &wxSystemInformationFrame::OnShowwxInfoMessageBox, this);
    buttonSizer->Add(wxInfoButton, wxSizerFlags().Border(wxRIGHT));

    wxButton* saveButton = new wxButton(mainPanel, wxID_ANY, _("Save..."));
    saveButton ->Bind(wxEVT_BUTTON, &wxSystemInformationFrame::OnSave, this);
    buttonSizer->Add(saveButton , wxSizerFlags().Border(wxRIGHT));

    // to move the button after it to the very right
    buttonSizer->AddStretchSpacer(1);

    wxButton* clearLogButton = new wxButton(mainPanel, wxID_ANY, _("Clear log"));
    clearLogButton->Bind(wxEVT_BUTTON, &wxSystemInformationFrame::OnClearLog, this);
    buttonSizer->Add(clearLogButton, wxSizerFlags().Border(wxRIGHT));

    mainPanelSizer->Add(buttonSizer, wxSizerFlags().Proportion(0).Expand().Border());

    // add a notebook with pages for groups of system information

    m_pages = new wxNotebook(mainPanel, wxID_ANY);

    if ( createFlags & ViewSystemColours )
        m_pages->AddPage(new SystemColourView(m_pages), _("wxSYS Colours"), true);

    if ( createFlags & ViewSystemFonts )
        m_pages->AddPage(new SystemFontView(m_pages), _("wxSYS Fonts"));

    if ( createFlags & ViewSystemMetrics )
        m_pages->AddPage(new SystemMetricView(m_pages), _("wxSYS Metrics"));

    if ( createFlags & ViewDisplays )
        m_pages->AddPage(new DisplaysView(m_pages), _("Displays"));

    if ( createFlags & ViewStandardPaths )
        m_pages->AddPage(new StandardPathsView(m_pages), _("Standard Paths"));

#if wxUSE_SYSTEM_OPTIONS
    if ( createFlags & ViewSystemOptions )
        m_pages->AddPage(new SystemOptionsView(m_pages), _("System Options"));
#endif

    if ( createFlags & ViewEnvironmentVariables )
        m_pages->AddPage(new EnvironmentVariablesView(m_pages), _("Environment Variables"));

    if ( createFlags & ViewMiscellaneous )
        m_pages->AddPage(new MiscellaneousView(m_pages), _("Miscellaneous"));

    if ( createFlags & ViewPreprocessorDefines )
        m_pages->AddPage(new PreprocessorDefinesView(m_pages), _("Preprocessor Defines"));

    wxASSERT_MSG(m_pages->GetPageCount() > 0, "Invalid createFlags: no View value specified");

    mainPanelSizer->Add(m_pages, wxSizerFlags().Proportion(5).Expand().Border());

    // Add a log control

    m_logCtrl = new wxTextCtrl(mainPanel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                                wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
    mainPanelSizer->Add(m_logCtrl, wxSizerFlags().Proportion(1).Expand().Border());

    for ( const auto& information : m_unloggedInformation )
        m_logCtrl->AppendText(information);
    m_unloggedInformation.clear();

    mainPanel->SetSizer(mainPanelSizer);

    if ( detailsButton )
        detailsButton->Bind(wxEVT_UPDATE_UI, &wxSystemInformationFrame::OnUpdateUI, this);

    return true;
}

wxArrayString wxSystemInformationFrame::GetValues(const wxString& separator) const
{
    wxArrayString values;

    // m_pages is null if the frame creation failed
    if ( !m_pages )
        return values;

    const size_t pageCount = m_pages->GetPageCount();

    for ( size_t i = 0; i < pageCount; ++i )
    {
        const SysInfoListView* view = dynamic_cast<SysInfoListView*>(m_pages->GetPage(i));

        if ( !view )
            continue;

        const wxArrayString viewValues = view->GetValues(separator);

        if ( i > 0 )
            values.push_back(wxEmptyString); // separate groups of values by an empty line

        values.push_back(m_pages->GetPageText(i));
        values.push_back("----------------------------");

        // not using values.insert() because it does not link when wxWidgets is built as a DLL
        for ( const auto& viewValue : viewValues )
            values.push_back(viewValue);
    }

    return values;
}

#ifdef __WXMSW__
WXLRESULT wxSystemInformationFrame::MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
{
    switch ( nMsg )
    {
        case WM_SETTINGCHANGE:
            // lParam is either null or a string identifying the changed setting
            LogInformation(wxString::Format("WM_SETTINGCHANGE received: wParam = %u, lParam =\"%s\"",
                (unsigned)wParam, lParam ? (LPCTSTR)lParam : wxS("")));
            TriggerValuesUpdate();
            break;

        case WM_THEMECHANGED:
            LogInformation(wxString::Format("WM_THEMECHANGED received: wParam = %#x, lParam = %#lx", (unsigned)wParam, (long)lParam));
            TriggerValuesUpdate();
            break;

#if !wxCHECK_VERSION(3, 1, 3) // 3.1.3+ has wxEVT_DPI_CHANGED
        case 0x02E0: // WM_DPICHANGED
            LogInformation(wxString::Format("WM_DPICHANGED received: new DPI = %u x %u",
                (unsigned)LOWORD(wParam), (unsigned)HIWORD(wParam)));
            TriggerValuesUpdate();
            break;
#endif // #if !wxCHECK_VERSION(3, 1, 3)
    }

    return wxFrame::MSWWindowProc(nMsg, wParam, lParam);
}
#endif // #ifdef __WXMSW__

void wxSystemInformationFrame::LogInformation(const wxString& information)
{
    wxString timeStampFormat = wxLog::GetTimestamp();
    wxString message;

    if ( timeStampFormat.empty() )
        timeStampFormat = "%c";

    message.Printf("%s: %s\n", wxDateTime::Now().Format(timeStampFormat), information);

    // LogInformation() can be called before m_logCtrl is created,
    // from overriden MSWWindowProc()
    if ( m_logCtrl )
        m_logCtrl->AppendText(message);
    else
        m_unloggedInformation.push_back(message);
}

void wxSystemInformationFrame::TriggerValuesUpdate()
{
    if ( !m_autoRefresh )
        return;

    // prevent multiple updates for a batch of setting change messages/events
    const int updateTimerDuration = 750; // milliseconds

    m_valuesUpdateTimer.StartOnce(updateTimerDuration);
}

void wxSystemInformationFrame::UpdateValues()
{
    if ( !m_pages )
        return;

    const size_t pageCount = m_pages->GetPageCount();

    {
        wxBusyCursor busyCursor;

        for ( size_t i = 0; i < pageCount; ++i )
        {
            SysInfoListView* view = dynamic_cast<SysInfoListView*>(m_pages->GetPage(i));

            if ( view )
                view->UpdateValues();
        }
    }

    LogInformation(_("System values were refreshed."));
}

void wxSystemInformationFrame::OnRefresh(wxCommandEvent&)
{
    UpdateValues();
}

void wxSystemInformationFrame::OnShowDetailedInformation(wxCommandEvent&)
{
    const SysInfoListView* view = dynamic_cast<SysInfoListView*>(m_pages->GetCurrentPage());

    if ( view )
        view->ShowDetailedInformation();
}

void wxSystemInformationFrame::OnShowwxInfoMessageBox(wxCommandEvent&)
{
    wxInfoMessageBox(this);
}

void wxSystemInformationFrame::OnSave(wxCommandEvent&)
{
    wxFileDialog dialog(this, _("Choose File Name"), wxEmptyString, wxEmptyString,
                        _("Text Files (*.txt)|*.txt"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    if ( dialog.ShowModal() != wxID_OK )
        return;

    wxFileName fileName(dialog.GetPath());

    // add the extension if the user did not specify any
    if ( !fileName.HasExt() )
        fileName.SetExt("txt");

    const wxString path = fileName.GetFullPath();
    wxTextFile textFile(path);

    if ( textFile.Exists() )
    {
        if ( !textFile.Open() )
        {
            wxLogError(_("Could not open file \"%s\"."), path);
            return;
        }
        textFile.Clear();
    }
    else
    if ( !textFile.Create() )
    {
        wxLogError(_("Could not create file \"%s\"."), path);
        return;
    }

    const wxArrayString values = GetValues();

    for ( const auto& value : values )
        textFile.AddLine(value);

    if ( !textFile.Write() )
        wxLogError(_("Could not write to file \"%s\"."), path);
}

void wxSystemInformationFrame::OnClearLog(wxCommandEvent&)
{
    m_logCtrl->Clear();
}

void wxSystemInformationFrame::OnUpdateUI(wxUpdateUIEvent& event)
{
    const SysInfoListView* view = dynamic_cast<SysInfoListView*>(m_pages->GetCurrentPage());

    event.Enable(view && view->CanShowDetailedInformation());
}

void wxSystemInformationFrame::OnUpdateValuesTimer(wxTimerEvent&)
{
    UpdateValues();
}

void wxSystemInformationFrame::OnSysColourChanged(wxSysColourChangedEvent& event)
{
    event.Skip();
    LogInformation(_("wxSysColourChangedEvent received."));
    TriggerValuesUpdate();
}

void wxSystemInformationFrame::OnDisplayChanged(wxDisplayChangedEvent& event)
{
    event.Skip();
    LogInformation(_("wxDisplayChangedEvent received."));
    TriggerValuesUpdate();
}

#if wxCHECK_VERSION(3, 1, 3)
void wxSystemInformationFrame::OnDPIChanged(wxDPIChangedEvent& event)
{
    event.Skip();
    LogInformation(wxString::Format(_("wxDPIChangedEvent received: old DPI = %s, new DPI = %s."),
        wxSizeTowxString(event.GetOldDPI()), wxSizeTowxString(event.GetNewDPI())));
    TriggerValuesUpdate();
}
#endif
