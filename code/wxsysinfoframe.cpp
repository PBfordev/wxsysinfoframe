/////////////////////////////////////////////////////////////////////////////
// Author:      PB
// Purpose:     Implementation of wxSystemInformationFrame and its helpers
// Copyright:   (c) 2019 PB <pbfordev@gmail.com>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wxsysinfoframe.h"

#include <map>

#include <wx/apptrait.h>
#include <wx/colordlg.h>
#include <wx/display.h>
#include <wx/dynlib.h>
#include <wx/filename.h>
#include <wx/fontdlg.h>
#include <wx/intl.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/settings.h>
#include <wx/stdpaths.h>
#include <wx/sysopt.h>
#include <wx/textfile.h>
#include <wx/utils.h>
#include <wx/wupdlock.h>

#if !wxCHECK_VERSION(3, 0, 0)
    #error wxSystemInformationFrame requires wxWidgets version 3 or higher
#endif


namespace { // anonymous namespace for helper classes

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

    virtual wxArrayString GetValues(const wxString& separator = " \t") const = 0;
protected:
    std::map<long,int> m_columnWidths;

    long AppendItemWithData(const wxString& label, long data);

    virtual void DoUpdateValues() = 0;
    virtual void DoShowDetailedInformation(long WXUNUSED(listItemIndex)) const {};

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

    DoUpdateValues();
    AutoSizeColumns();

    if ( GetFirstSelected() == -1 && GetItemCount() > 0 )
    {
        Select(0);
        Focus(0);
    }
}

void SysInfoListView::ShowDetailedInformation() const
{
    DoShowDetailedInformation(GetFirstSelected());
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
    for ( int i = 0; i < GetColumnCount(); ++i )
    {
        auto it = m_columnWidths.find(i);

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
    ~SystemColourView();

    bool CanShowDetailedInformation() const override { return GetFirstSelected() != -1; }

    void SetColourBitmapOutlineColour(const wxColour& outlineColour);
    wxColour GetColourBitmapOutlineColour() const { return m_outlineColour; }

    static wxColour GetDefaultColourBitmapOutlineColour();
protected:
    void DoUpdateValues() override;
    void DoShowDetailedInformation(long listItemIndex) const override;
private:
    wxImageList* m_imageList{nullptr};
    wxColour m_outlineColour;

    wxBitmap CreateColourBitmap(const wxColour& colour, const wxSize& size);
};

struct ColourInfo
{
    wxSystemColour index;
    const char*    name;
    const char*    description;
};

ColourInfo const s_colourInfoArray[] =
{
    wxSYS_COLOUR_SCROLLBAR, "wxSYS_COLOUR_SCROLLBAR", "The scrollbar grey area.",
    wxSYS_COLOUR_DESKTOP, "wxSYS_COLOUR_DESKTOP", "The desktop colour.",
    wxSYS_COLOUR_ACTIVECAPTION, "wxSYS_COLOUR_ACTIVECAPTION", "Active window caption colour.",
    wxSYS_COLOUR_INACTIVECAPTION, "wxSYS_COLOUR_INACTIVECAPTION", "Inactive window caption colour.",
    wxSYS_COLOUR_MENU, "wxSYS_COLOUR_MENU", "Menu background colour.",
    wxSYS_COLOUR_WINDOW, "wxSYS_COLOUR_WINDOW", "Window background colour.",
    wxSYS_COLOUR_WINDOWFRAME, "wxSYS_COLOUR_WINDOWFRAME", "Window frame colour.",
    wxSYS_COLOUR_MENUTEXT, "wxSYS_COLOUR_MENUTEXT", "Colour of the text used in the menus.",
    wxSYS_COLOUR_WINDOWTEXT, "wxSYS_COLOUR_WINDOWTEXT", "Colour of the text used in generic windows.",
    wxSYS_COLOUR_CAPTIONTEXT, "wxSYS_COLOUR_CAPTIONTEXT", "Colour of the text used in captions, size boxes and scrollbar arrow boxes.",
    wxSYS_COLOUR_ACTIVEBORDER, "wxSYS_COLOUR_ACTIVEBORDER", "Active window border colour.",
    wxSYS_COLOUR_INACTIVEBORDER, "wxSYS_COLOUR_INACTIVEBORDER", "Inactive window border colour.",
    wxSYS_COLOUR_APPWORKSPACE, "wxSYS_COLOUR_APPWORKSPACE", "Background colour for MDI applications.",
    wxSYS_COLOUR_HIGHLIGHT, "wxSYS_COLOUR_HIGHLIGHT", "Colour of item(s) selected in a control.",
    wxSYS_COLOUR_HIGHLIGHTTEXT, "wxSYS_COLOUR_HIGHLIGHTTEXT", "Colour of the text of item(s) selected in a control.",
    wxSYS_COLOUR_BTNFACE, "wxSYS_COLOUR_BTNFACE", "Face shading colour on push buttons.",
    wxSYS_COLOUR_BTNSHADOW, "wxSYS_COLOUR_BTNSHADOW", "Edge shading colour on push buttons.",
    wxSYS_COLOUR_GRAYTEXT, "wxSYS_COLOUR_GRAYTEXT", "Colour of greyed (disabled) text.",
    wxSYS_COLOUR_BTNTEXT, "wxSYS_COLOUR_BTNTEXT", "Colour of the text on push buttons.",
    wxSYS_COLOUR_INACTIVECAPTIONTEXT, "wxSYS_COLOUR_INACTIVECAPTIONTEXT", "Colour of the text in active captions.",
    wxSYS_COLOUR_BTNHIGHLIGHT, "wxSYS_COLOUR_BTNHIGHLIGHT", "Highlight colour for buttons.",
    wxSYS_COLOUR_3DDKSHADOW, "wxSYS_COLOUR_3DDKSHADOW", "Dark shadow colour for three-dimensional display elements.",
    wxSYS_COLOUR_3DLIGHT, "wxSYS_COLOUR_3DLIGHT", "Light colour for three-dimensional display elements.",
    wxSYS_COLOUR_INFOTEXT, "wxSYS_COLOUR_itemTEXT", "Text colour for tooltip controls.",
    wxSYS_COLOUR_INFOBK, "wxSYS_COLOUR_itemBK", "Background colour for tooltip controls.",
    wxSYS_COLOUR_LISTBOX, "wxSYS_COLOUR_LISTBOX", "Background colour for list-like controls.",
    wxSYS_COLOUR_HOTLIGHT, "wxSYS_COLOUR_HOTLIGHT", "Colour for a hyperlink or hot-tracked item.",
    wxSYS_COLOUR_GRADIENTACTIVECAPTION, "wxSYS_COLOUR_GRADIENTACTIVECAPTION", "Right side colour in the color gradient of an active window's title bar.",
    wxSYS_COLOUR_GRADIENTINACTIVECAPTION, "wxSYS_COLOUR_GRADIENTINACTIVECAPTION", "Right side colour in the color gradient of an inactive window's title bar.",
    wxSYS_COLOUR_MENUHILIGHT, "wxSYS_COLOUR_MENUHILIGHT", "The colour used to highlight menu items when the menu appears as a flat menu.",
    wxSYS_COLOUR_MENUBAR, "wxSYS_COLOUR_MENUBAR", "The background colour for the menu bar when menus appear as flat menus.",
    wxSYS_COLOUR_LISTBOXTEXT, "wxSYS_COLOUR_LISTBOXTEXT", "Text colour for list-like controls.",
    wxSYS_COLOUR_LISTBOXHIGHLIGHTTEXT, "wxSYS_COLOUR_LISTBOXHIGHLIGHTTEXT", "Text colour for the unfocused selection of list-like controls.",
    wxSYS_COLOUR_BACKGROUND, "wxSYS_COLOUR_BACKGROUND", "Synonym for wxSYS_COLOUR_DESKTOP.",
    wxSYS_COLOUR_3DFACE, "wxSYS_COLOUR_3DFACE", "Synonym for wxSYS_COLOUR_BTNFACE.",
    wxSYS_COLOUR_3DSHADOW, "wxSYS_COLOUR_3DSHADOW", "Synonym for wxSYS_COLOUR_BTNSHADOW.",
    wxSYS_COLOUR_BTNHILIGHT, "wxSYS_COLOUR_BTNHILIGHT", "Synonym for wxSYS_COLOUR_BTNHIGHLIGHT.",
    wxSYS_COLOUR_3DHIGHLIGHT, "wxSYS_COLOUR_3DHIGHLIGHT", "Synonym for wxSYS_COLOUR_BTNHIGHLIGHT.",
    wxSYS_COLOUR_3DHILIGHT, "wxSYS_COLOUR_3DHILIGHT", "Synonym for wxSYS_COLOUR_BTNHIGHLIGHT.",
    wxSYS_COLOUR_FRAMEBK, "wxSYS_COLOUR_FRAMEBK", "Synonym for wxSYS_COLOUR_BTNFACE.",
};

SystemColourView::SystemColourView(wxWindow* parent)
    : SystemSettingView(parent)
{
    SetColourBitmapOutlineColour(GetDefaultColourBitmapOutlineColour());

    for ( size_t i = 0; i < WXSIZEOF(s_colourInfoArray); ++i )
    {
         const wxString colourName = s_colourInfoArray[i].name;
         const wxString colourDescription = s_colourInfoArray[i].description;
         const long itemIndex = InsertItem(i, colourName, -1);

         SetItem(itemIndex, Column_Description, colourDescription);
         SetItemData(itemIndex, (long)i);
    }

    UpdateValues();
}

SystemColourView::~SystemColourView()
{
    if ( m_imageList )
        delete m_imageList;
}

void SystemColourView::SetColourBitmapOutlineColour(const wxColour& outlineColour)
{
    m_outlineColour = outlineColour;
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

    if ( m_imageList )
    {
        SetImageList(nullptr, wxIMAGE_LIST_SMALL);
        delete m_imageList;
    }

    m_imageList = new wxImageList();

    size.SetWidth(wxSystemSettings::GetMetric(wxSYS_SMALLICON_X, this));
    size.SetHeight(wxSystemSettings::GetMetric(wxSYS_SMALLICON_Y, this));
    m_imageList->Create(size.GetWidth(), size.GetHeight(), false);
    SetImageList(m_imageList, wxIMAGE_LIST_SMALL);

    const int itemCount = GetItemCount();

    for ( int i = 0; i < itemCount; ++i )
    {
         const wxColour colour = wxSystemSettings::GetColour(s_colourInfoArray[GetItemData(i)].index);
         const int imageIndex = m_imageList->Add(CreateColourBitmap(colour.IsOk() ? colour : GetColourBitmapOutlineColour(), size));
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

    wxGetColourFromUser(GetParent(), colour, wxString::Format(_("Viewing %s"), valueName), &colourData);
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
    wxSYS_OEM_FIXED_FONT, "wxSYS_OEM_FIXED_FONT", "Original equipment manufacturer dependent fixed-pitch font.",
    wxSYS_ANSI_FIXED_FONT, "wxSYS_ANSI_FIXED_FONT", "Windows fixed-pitch (monospaced) font.",
    wxSYS_ANSI_VAR_FONT, "wxSYS_ANSI_VAR_FONT", "Windows variable-pitch (proportional) font.",
    wxSYS_SYSTEM_FONT, "wxSYS_SYSTEM_FONT", "System font. By default, the system uses the system font to draw menus, dialog box controls, and text.",
    wxSYS_DEVICE_DEFAULT_FONT, "wxSYS_DEVICE_DEFAULT_FONT", "Device-dependent font.",
    wxSYS_DEFAULT_GUI_FONT, "wxSYS_DEFAULT_GUI_FONT", "Default font for user interface objects such as menus and dialog boxes.",
};

SystemFontView::SystemFontView(wxWindow* parent)
    : SystemSettingView(parent)
{
    for ( size_t i = 0; i < WXSIZEOF(s_fontInfoArray); ++i )
    {
         const wxString fontName = s_fontInfoArray[i].name;
         const wxString fontDescription = s_fontInfoArray[i].description;
         const long itemIndex = AppendItemWithData(fontName, (long)i);

         SetItem(itemIndex, Column_Description, fontDescription);
    }

    UpdateValues();
}

void SystemFontView::DoUpdateValues()
{
    const int itemCount = GetItemCount();

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

    wxGetFontFromUser(GetParent(), font, wxString::Format(_("Viewing %s"), valueName));
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
    wxSYS_MOUSE_BUTTONS, "wxSYS_MOUSE_BUTTONS", "Number of buttons on mouse, or zero if no mouse was installed.",
    wxSYS_BORDER_X, "wxSYS_BORDER_X", "Width of single border.",
    wxSYS_BORDER_Y, "wxSYS_BORDER_Y", "Height of single border.",
    wxSYS_CURSOR_X, "wxSYS_CURSOR_X", "Width of cursor.",
    wxSYS_CURSOR_Y, "wxSYS_CURSOR_Y", "Height of cursor.",
    wxSYS_DCLICK_X, "wxSYS_DCLICK_X", "Width in pixels of rectangle within which two successive mouse clicks must fall to generate a double-click.",
    wxSYS_DCLICK_Y, "wxSYS_DCLICK_Y", "Height in pixels of rectangle within which two successive mouse clicks must fall to generate a double-click.",
    wxSYS_DRAG_X, "wxSYS_DRAG_X", "Width in pixels of a rectangle centered on a drag point to allow for limited movement of the mouse pointer before a drag operation begins.",
    wxSYS_DRAG_Y, "wxSYS_DRAG_Y", "Height in pixels of a rectangle centered on a drag point to allow for limited movement of the mouse pointer before a drag operation begins.",
    wxSYS_EDGE_X, "wxSYS_EDGE_X", "Width of a 3D border, in pixels.",
    wxSYS_EDGE_Y, "wxSYS_EDGE_Y", "Height of a 3D border, in pixels.",
    wxSYS_HSCROLL_ARROW_X, "wxSYS_HSCROLL_ARROW_X", "Width of arrow bitmap on horizontal scrollbar.",
    wxSYS_HSCROLL_ARROW_Y, "wxSYS_HSCROLL_ARROW_Y", "Height of arrow bitmap on horizontal scrollbar.",
    wxSYS_HTHUMB_X, "wxSYS_HTHUMB_X", "Width of horizontal scrollbar thumb.",
    wxSYS_ICON_X, "wxSYS_ICON_X", "The default width of an icon.",
    wxSYS_ICON_Y, "wxSYS_ICON_Y", "The default height of an icon.",
    wxSYS_ICONSPACING_X, "wxSYS_ICONSPACING_X", "Width of a grid cell for items in large icon view, in pixels. Each item fits into a rectangle of this size when arranged.",
    wxSYS_ICONSPACING_Y, "wxSYS_ICONSPACING_Y", "Height of a grid cell for items in large icon view, in pixels. Each item fits into a rectangle of this size when arranged.",
    wxSYS_WINDOWMIN_X, "wxSYS_WINDOWMIN_X", "Minimum width of a window.",
    wxSYS_WINDOWMIN_Y, "wxSYS_WINDOWMIN_Y", "Minimum height of a window.",
    wxSYS_SCREEN_X, "wxSYS_SCREEN_X", "Width of the screen in pixels.",
    wxSYS_SCREEN_Y, "wxSYS_SCREEN_Y", "Height of the screen in pixels.",
    wxSYS_FRAMESIZE_X, "wxSYS_FRAMESIZE_X", "Width of the window frame for a wxTHICK_FRAME window.",
    wxSYS_FRAMESIZE_Y, "wxSYS_FRAMESIZE_Y", "Height of the window frame for a wxTHICK_FRAME window.",
    wxSYS_SMALLICON_X, "wxSYS_SMALLICON_X", "Recommended width of a small icon (in window captions, and small icon view).",
    wxSYS_SMALLICON_Y, "wxSYS_SMALLICON_Y", "Recommended height of a small icon (in window captions, and small icon view).",
    wxSYS_HSCROLL_Y, "wxSYS_HSCROLL_Y", "Height of horizontal scrollbar in pixels.",
    wxSYS_VSCROLL_X, "wxSYS_VSCROLL_X", "Width of vertical scrollbar in pixels.",
    wxSYS_VSCROLL_ARROW_X, "wxSYS_VSCROLL_ARROW_X", "Width of arrow bitmap on a vertical scrollbar.",
    wxSYS_VSCROLL_ARROW_Y, "wxSYS_VSCROLL_ARROW_Y", "Height of arrow bitmap on a vertical scrollbar.",
    wxSYS_VTHUMB_Y, "wxSYS_VTHUMB_Y", "Height of vertical scrollbar thumb.",
    wxSYS_CAPTION_Y, "wxSYS_CAPTION_Y", "Height of normal caption area.",
    wxSYS_MENU_Y, "wxSYS_MENU_Y", "Height of single-line menu bar.",
    wxSYS_NETWORK_PRESENT, "wxSYS_NETWORK_PRESENT", "1 if there is a network present, 0 otherwise.",
    wxSYS_PENWINDOWS_PRESENT, "wxSYS_PENWINDOWS_PRESENT", "1 if PenWindows is installed, 0 otherwise.",
    wxSYS_SHOW_SOUNDS, "wxSYS_SHOW_SOUNDS", "Non-zero if the user requires an application to present information visually in situations where it would otherwise present the information only in audible form; zero otherwise.",
    wxSYS_SWAP_BUTTONS, "wxSYS_SWAP_BUTTONS", "Non-zero if the meanings of the left and right mouse buttons are swapped; zero otherwise.",
    wxSYS_DCLICK_MSEC, "wxSYS_DCLICK_MSEC", "Maximal time, in milliseconds, which may pass between subsequent clicks for a double click to be generated.",

// there was a bug with these three metrics on wxMSW
// until commit e5d850d (commited on Apr 18, 2019)
#if wxCHECK_VERSION(3, 1, 1) && ( !defined(__WXMSW__) || wxCHECK_VERSION(3, 1, 3)  )
    wxSYS_CARET_ON_MSEC, "wxSYS_CARET_ON_MSEC", "Time, in milliseconds, for how long a blinking caret should stay visible during a single blink cycle before it disappears.",
    wxSYS_CARET_OFF_MSEC, "wxSYS_CARET_OFF_MSEC", "Time, in milliseconds, for how long a blinking caret should stay invisible during a single blink cycle before it reappears. If this value is zero, carets should be visible all the time instead of blinking. If the value is negative, the platform does not support the user setting. Implemented only on GTK+ and MacOS X.",
    wxSYS_CARET_TIMEOUT_MSEC, "wxSYS_CARET_TIMEOUT_MSEC", "Time, in milliseconds, for how long a caret should blink after a user interaction. After this timeout has expired, the caret should stay continuously visible until the user interacts with the caret again (for example by entering, deleting or cutting text). If this value is negative, carets should blink forever; if it is zero, carets should not blink at all.",
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
         const int metricValue  = wxSystemSettings::GetMetric(s_metricInfoArray[GetItemData(i)].index, GetGrandParent());

         SetItem(i, Column_Value, wxString::Format("%d", metricValue));
    }
}


/*************************************************

    DisplaysView

*************************************************/

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
        Param_IsPrimary,
        Param_Resolution,
        Param_BPP,
        Param_Frequency,
        Param_GeometryCoords,
        Param_GeometrySize,
        Param_ClientAreaCoords,
        Param_ClientAreaSize,
        Param_PPI,
    };
};

DisplaysView::DisplaysView(wxWindow* parent)
    : SysInfoListView(parent)
{
    AppendColumn(_("Parameter"));

    AppendItemWithData(_("Name"), Param_Name);
    AppendItemWithData(_("Is Primary"), Param_IsPrimary);
    AppendItemWithData(_("Resolution"), Param_Resolution);
    AppendItemWithData(_("Bits Per Pixel"), Param_BPP);
    AppendItemWithData(_("Refresh Frequency (Hz)"), Param_Frequency);
    AppendItemWithData(_("Geometry Coordinates (left, top; right, bottom)"), Param_GeometryCoords);
    AppendItemWithData(_("Geometry Size"), Param_GeometrySize);
    AppendItemWithData(_("Client Area Coordinates (left, top; right, bottom)"), Param_ClientAreaCoords);
    AppendItemWithData(_("Client Area Size"), Param_ClientAreaSize);
    AppendItemWithData(_("Pixels Per Inch"), Param_PPI);

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

    for ( int i = 1; i < columnCount; ++i )
    {
        wxListItem listItem;

        listItem.SetMask(wxLIST_MASK_TEXT);
        GetColumn(i, listItem);
        s += separator + listItem.GetText();
    }
    values.push_back(s);

    // dump values
    for ( int i = 0; i < itemCount; ++i )
    {
        s = GetItemText(i, 0);
        for ( int i1 = 1; i1 < columnCount; ++i1 )
        {
            s += separator + GetItemText(i, i1);
        }
        values.push_back(s);
    }

    return values;
}

wxString wxRectToxwString(const wxRect& r)
{
    return wxString::Format(_("%d, %d; %d, %d"),
        r.GetLeft(), r.GetTop(), r.GetRight(), r.GetBottom());
}

void DisplaysView::DoUpdateValues()
{
    while ( GetColumnCount() > 1 )
        DeleteColumn(1);

    const unsigned int displayCount = wxDisplay::GetCount();
    const int itemCount = GetItemCount();

    for ( size_t displayIndex = 0; displayIndex < displayCount; ++displayIndex )
    {
        const wxDisplay display(displayIndex);
        const wxVideoMode videoMode = display.GetCurrentMode();
        const wxRect geometryCoords = display.GetGeometry();
        const wxRect clientAreaCoords = display.GetClientArea();
        const wxSize ppi = display.GetPPI();
        const int columnIndex = displayIndex + 1;

        AppendColumn(wxString::Format("wxDisplay(%zu)", displayIndex));

        for ( int itemIndex = 0; itemIndex < itemCount; ++itemIndex )
        {
            const int param = GetItemData(itemIndex);
            wxString value;

            switch ( param )
            {
                case Param_Name:
                    value = display.GetName();
                    break;
                case Param_IsPrimary:
                    value =  display.IsPrimary() ? _("Yes") : _("No");
                    break;
                case Param_Resolution:
                    value.Printf(_("%d x %d"), videoMode.GetWidth(), videoMode.GetHeight());
                    break;
                case Param_BPP:
                    value.Printf("%d", videoMode.GetDepth());
                    break;
                case Param_Frequency:
                    value.Printf("%d", videoMode.refresh);
                    break;
                case Param_GeometryCoords:
                    value = wxRectToxwString(geometryCoords);
                    break;
                case Param_GeometrySize:
                    value.Printf(_("%d x %d"), geometryCoords.GetWidth(), geometryCoords.GetHeight());
                    break;
                case Param_ClientAreaCoords:
                    value = wxRectToxwString(clientAreaCoords);
                    break;
                case Param_ClientAreaSize:
                    value.Printf(_("%d x %d"), clientAreaCoords.GetWidth(), clientAreaCoords.GetHeight());
                    break;
                case Param_PPI:
                    value.Printf(_("%d x %d"), ppi.GetWidth(), ppi.GetHeight());
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

    enum
    {
        Param_ExitOnAssert = 0,

        // MSW options
        Param_NoMaskBit,
        Param_MSWRemap,
        Param_MSWWindowNoClipChildren,
        Param_MSWNotebookThemedBackground,
        Param_MSWStaticBoxOptimizedPaint,
        Param_MSWFontNoProofQuality,

        // GTK options
        Param_GTKTLWCanSetTransparent,
        Param_GTKDesktop,
        Param_GTKWindowForceBackgroundColour,

        // MAC options
        Param_MacWindowPlainTransition,
        Param_MacWindowDefaultVariant,
        Param_MacListCtrlAwaysUseGeneric,
        Param_MacTextControlUseSpellChecker,
        Param_OSXFileDialogAlwaysShowTypes,
    };
};

SystemOptionsView::SystemOptionsView(wxWindow* parent)
    : SysInfoListView(parent)
{
    InsertColumn(Column_Name, _("Name"));
    InsertColumn(Column_Value, _("Value"));

    AppendItemWithData(wxS("exit-on-assert"), Param_ExitOnAssert);

#ifdef __WXMSW__
    AppendItemWithData(wxS("no-maskblt"), Param_NoMaskBit);
    AppendItemWithData(wxS("msw.remap"), Param_MSWRemap);
    AppendItemWithData(wxS("msw.window.no-clip-children"), Param_MSWWindowNoClipChildren);
    AppendItemWithData(wxS("msw.notebook.themed-background"), Param_MSWNotebookThemedBackground);
    AppendItemWithData(wxS("msw.staticbox.optimized-paint"), Param_MSWStaticBoxOptimizedPaint);
    AppendItemWithData(wxS("msw.font.no-proof-quality"), Param_MSWFontNoProofQuality);
#endif // #ifdef __WXMSW__

#ifdef __WXGTK__
    AppendItemWithData(wxS("gtk.tlw.can-set-transparent"), Param_GTKTLWCanSetTransparent);
    AppendItemWithData(wxS("gtk.desktop"), Param_GTKDesktop);
    AppendItemWithData(wxS("gtk.window.force-background-colour"), Param_GTKWindowForceBackgroundColour);
#endif // #ifdef __WXGTK__

#ifdef __WXMAC__
    AppendItemWithData(wxS("mac.window-plain-transition"), Param_MacWindowPlainTransition);
    AppendItemWithData(wxS("window-default-variant"), Param_MacWindowDefaultVariant);
    AppendItemWithData(wxS("mac.listctrl.always_use_generic"), Param_MacListCtrlAwaysUseGeneric);
    AppendItemWithData(wxS("mac.textcontrol-use-spell-checker"), Param_MacTextControlUseSpellChecker);
    AppendItemWithData(wxS("osx.openfiledialog.always-show-types"), Param_OSXFileDialogAlwaysShowTypes);
#endif // #ifdef __WXMAC__

    UpdateValues();
}

wxString SysOptToString(const wxString& name)
{
    if ( !wxSystemOptions::HasOption(name) )
        return _("<Not Set>");

    return wxSystemOptions::GetOption(name);
}

void SystemOptionsView::DoUpdateValues()
{
    const long itemCount = GetItemCount();

    for ( int i = 0; i < itemCount; ++i )
    {
        const long param = GetItemData(i);
        wxString value;

        switch ( param )
        {
            case Param_ExitOnAssert: value = SysOptToString(wxS("exit-on-assert")); break;
#ifdef __WXMSW__
            case Param_NoMaskBit:                   value = SysOptToString(wxS("no-maskblt")); break;
            case Param_MSWRemap:                    value = SysOptToString(wxS("msw.remap")); break;
            case Param_MSWWindowNoClipChildren:     value = SysOptToString(wxS("msw.window.no-clip-children")); break;
            case Param_MSWNotebookThemedBackground: value = SysOptToString(wxS("msw.notebook.themed-background")); break;
            case Param_MSWStaticBoxOptimizedPaint:  value = SysOptToString(wxS("msw.staticbox.optimized-paint")); break;
            case Param_MSWFontNoProofQuality:       value = SysOptToString(wxS("msw.font.no-proof-quality")); break;
#endif // #ifdef __WXMSW__

#ifdef __WXGTK__
            case Param_GTKTLWCanSetTransparent:        value = SysOptToString(wxS("gtk.tlw.can-set-transparent")); break;
            case Param_GTKDesktop:                     value = SysOptToString(wxS("gtk.desktop")); break;
            case Param_GTKWindowForceBackgroundColour: value = SysOptToString(wxS("gtk.window.force-background-colour")); break;
#endif // #ifdef __WXGTK__

#ifdef __WXMAC__
            case Param_MacWindowPlainTransition:      value = SysOptToString(wxS("mac.window-plain-transition")); break;
            case Param_MacWindowDefaultVariant:       value = SysOptToString(wxS("window-default-variant")); break;
            case Param_MacListCtrlAwaysUseGeneric:    value = SysOptToString(wxS("mac.listctrl.always_use_generic")); break;
            case Param_MacTextControlUseSpellChecker: value = SysOptToString(wxS("mac.textcontrol-use-spell-checker")); break;
            case Param_OSXFileDialogAlwaysShowTypes:  value = SysOptToString(wxS("osx.openfiledialog.always-show-types")); break;
#endif // #ifdef __WXMAC__

            default:
                wxFAIL;
        }

        SetItem(i, Column_Value, value);
    }
}


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

        // the group below is available only for wxMSW
        Param_CSIDL_DESKTOP,
        Param_CSIDL_INTERNET,
        Param_CSIDL_PROGRAMS,
        Param_CSIDL_CONTROLS,
        Param_CSIDL_PRINTERS,
        Param_CSIDL_FAVORITES,
        Param_CSIDL_STARTUP,
        Param_CSIDL_RECENT,
        Param_CSIDL_SENDTO,
        Param_CSIDL_BITBUCKET,
        Param_CSIDL_STARTMENU,
        Param_CSIDL_MYDOCUMENTS,
        Param_CSIDL_MYMUSIC,
        Param_CSIDL_MYVIDEO,
        Param_CSIDL_DESKTOPDIRECTORY,
        Param_CSIDL_DRIVES,
        Param_CSIDL_NETWORK,
        Param_CSIDL_NETHOOD,
        Param_CSIDL_FONTS,
        Param_CSIDL_TEMPLATES,
        Param_CSIDL_COMMON_STARTMENU,
        Param_CSIDL_COMMON_PROGRAMS,
        Param_CSIDL_COMMON_STARTUP,
        Param_CSIDL_COMMON_DESKTOPDIRECTORY,
        Param_CSIDL_APPDATA,
        Param_CSIDL_PRINTHOOD,
        Param_CSIDL_LOCAL_APPDATA,
        Param_CSIDL_ALTSTARTUP,
        Param_CSIDL_COMMON_ALTSTARTUP,
        Param_CSIDL_COMMON_FAVORITES,
        Param_CSIDL_INTERNET_CACHE,
        Param_CSIDL_COOKIES,
        Param_CSIDL_HISTORY,
        Param_CSIDL_COMMON_APPDATA,
        Param_CSIDL_WINDOWS,
        Param_CSIDL_SYSTEM,
        Param_CSIDL_PROGRAM_FILES,
        Param_CSIDL_MYPICTURES,
        Param_CSIDL_PROFILE,
        Param_CSIDL_SYSTEMX86,
        Param_CSIDL_PROGRAM_FILESX86,
        Param_CSIDL_PROGRAM_FILES_COMMON,
        Param_CSIDL_PROGRAM_FILES_COMMONX86,
        Param_CSIDL_COMMON_TEMPLATES,
        Param_CSIDL_COMMON_DOCUMENTS,
        Param_CSIDL_COMMON_ADMINTOOLS,
        Param_CSIDL_ADMINTOOLS,
        Param_CSIDL_CONNECTIONS,
        Param_CSIDL_COMMON_MUSIC,
        Param_CSIDL_COMMON_PICTURES,
        Param_CSIDL_COMMON_VIDEO,
        Param_CSIDL_RESOURCES,
        Param_CSIDL_RESOURCES_LOCALIZED,
        Param_CSIDL_COMMON_OEM_LINKS,
        Param_CSIDL_COMPUTERSNEARME,

        // GTK only
        Param_InstallPrefix,
    };
};

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
    AppendItemWithData("MSWShellDir CSIDL_DESKTOP", Param_CSIDL_DESKTOP);
    AppendItemWithData("MSWShellDir CSIDL_INTERNET", Param_CSIDL_INTERNET);
    AppendItemWithData("MSWShellDir CSIDL_PROGRAMS", Param_CSIDL_PROGRAMS);
    AppendItemWithData("MSWShellDir CSIDL_CONTROLS", Param_CSIDL_CONTROLS);
    AppendItemWithData("MSWShellDir CSIDL_PRINTERS", Param_CSIDL_PRINTERS);
    AppendItemWithData("MSWShellDir CSIDL_FAVORITES", Param_CSIDL_FAVORITES);
    AppendItemWithData("MSWShellDir CSIDL_STARTUP", Param_CSIDL_STARTUP);
    AppendItemWithData("MSWShellDir CSIDL_RECENT", Param_CSIDL_RECENT);
    AppendItemWithData("MSWShellDir CSIDL_SENDTO", Param_CSIDL_SENDTO);
    AppendItemWithData("MSWShellDir CSIDL_BITBUCKET", Param_CSIDL_BITBUCKET);
    AppendItemWithData("MSWShellDir CSIDL_STARTMENU", Param_CSIDL_STARTMENU);
    AppendItemWithData("MSWShellDir CSIDL_MYDOCUMENTS", Param_CSIDL_MYDOCUMENTS);
    AppendItemWithData("MSWShellDir CSIDL_MYMUSIC", Param_CSIDL_MYMUSIC);
    AppendItemWithData("MSWShellDir CSIDL_MYVIDEO", Param_CSIDL_MYVIDEO);
    AppendItemWithData("MSWShellDir CSIDL_DESKTOPDIRECTORY", Param_CSIDL_DESKTOPDIRECTORY);
    AppendItemWithData("MSWShellDir CSIDL_DRIVES", Param_CSIDL_DRIVES);
    AppendItemWithData("MSWShellDir CSIDL_NETWORK", Param_CSIDL_NETWORK);
    AppendItemWithData("MSWShellDir CSIDL_NETHOOD", Param_CSIDL_NETHOOD);
    AppendItemWithData("MSWShellDir CSIDL_FONTS", Param_CSIDL_FONTS);
    AppendItemWithData("MSWShellDir CSIDL_TEMPLATES", Param_CSIDL_TEMPLATES);
    AppendItemWithData("MSWShellDir CSIDL_COMMON_STARTMENU", Param_CSIDL_COMMON_STARTMENU);
    AppendItemWithData("MSWShellDir CSIDL_COMMON_PROGRAMS", Param_CSIDL_COMMON_PROGRAMS);
    AppendItemWithData("MSWShellDir CSIDL_COMMON_STARTUP", Param_CSIDL_COMMON_STARTUP);
    AppendItemWithData("MSWShellDir CSIDL_COMMON_DESKTOPDIRECTORY", Param_CSIDL_COMMON_DESKTOPDIRECTORY);
    AppendItemWithData("MSWShellDir CSIDL_APPDATA", Param_CSIDL_APPDATA);
    AppendItemWithData("MSWShellDir CSIDL_PRINTHOOD", Param_CSIDL_PRINTHOOD);
    AppendItemWithData("MSWShellDir CSIDL_LOCAL_APPDATA", Param_CSIDL_LOCAL_APPDATA);
    AppendItemWithData("MSWShellDir CSIDL_ALTSTARTUP", Param_CSIDL_ALTSTARTUP);
    AppendItemWithData("MSWShellDir CSIDL_COMMON_ALTSTARTUP", Param_CSIDL_COMMON_ALTSTARTUP);
    AppendItemWithData("MSWShellDir CSIDL_COMMON_FAVORITES", Param_CSIDL_COMMON_FAVORITES);
    AppendItemWithData("MSWShellDir CSIDL_INTERNET_CACHE", Param_CSIDL_INTERNET_CACHE);
    AppendItemWithData("MSWShellDir CSIDL_COOKIES", Param_CSIDL_COOKIES);
    AppendItemWithData("MSWShellDir CSIDL_HISTORY", Param_CSIDL_HISTORY);
    AppendItemWithData("MSWShellDir CSIDL_COMMON_APPDATA", Param_CSIDL_COMMON_APPDATA);
    AppendItemWithData("MSWShellDir CSIDL_WINDOWS", Param_CSIDL_WINDOWS);
    AppendItemWithData("MSWShellDir CSIDL_SYSTEM", Param_CSIDL_SYSTEM);
    AppendItemWithData("MSWShellDir CSIDL_PROGRAM_FILES", Param_CSIDL_PROGRAM_FILES);
    AppendItemWithData("MSWShellDir CSIDL_MYPICTURES", Param_CSIDL_MYPICTURES);
    AppendItemWithData("MSWShellDir CSIDL_PROFILE", Param_CSIDL_PROFILE);
    AppendItemWithData("MSWShellDir CSIDL_SYSTEMX86", Param_CSIDL_SYSTEMX86);
    AppendItemWithData("MSWShellDir CSIDL_PROGRAM_FILESX86", Param_CSIDL_PROGRAM_FILESX86);
    AppendItemWithData("MSWShellDir CSIDL_PROGRAM_FILES_COMMON", Param_CSIDL_PROGRAM_FILES_COMMON);
    AppendItemWithData("MSWShellDir CSIDL_PROGRAM_FILES_COMMONX86", Param_CSIDL_PROGRAM_FILES_COMMONX86);
    AppendItemWithData("MSWShellDir CSIDL_COMMON_TEMPLATES", Param_CSIDL_COMMON_TEMPLATES);
    AppendItemWithData("MSWShellDir CSIDL_COMMON_DOCUMENTS", Param_CSIDL_COMMON_DOCUMENTS);
    AppendItemWithData("MSWShellDir CSIDL_COMMON_ADMINTOOLS", Param_CSIDL_COMMON_ADMINTOOLS);
    AppendItemWithData("MSWShellDir CSIDL_ADMINTOOLS", Param_CSIDL_ADMINTOOLS);
    AppendItemWithData("MSWShellDir CSIDL_CONNECTIONS", Param_CSIDL_CONNECTIONS);
    AppendItemWithData("MSWShellDir CSIDL_COMMON_MUSIC", Param_CSIDL_COMMON_MUSIC);
    AppendItemWithData("MSWShellDir CSIDL_COMMON_PICTURES", Param_CSIDL_COMMON_PICTURES);
    AppendItemWithData("MSWShellDir CSIDL_COMMON_VIDEO", Param_CSIDL_COMMON_VIDEO);
    AppendItemWithData("MSWShellDir CSIDL_RESOURCES", Param_CSIDL_RESOURCES);
    AppendItemWithData("MSWShellDir CSIDL_RESOURCES_LOCALIZED", Param_CSIDL_RESOURCES_LOCALIZED);
    AppendItemWithData("MSWShellDir CSIDL_COMMON_OEM_LINKS", Param_CSIDL_COMMON_OEM_LINKS);
    AppendItemWithData("MSWShellDir CSIDL_COMPUTERSNEARME", Param_CSIDL_COMPUTERSNEARME);
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

#ifdef __WXMSW__
            case Param_CSIDL_DESKTOP:                 value = paths.MSWGetShellDir(0x0000 | flags); break;
            case Param_CSIDL_INTERNET:                value = paths.MSWGetShellDir(0x0001 | flags); break;
            case Param_CSIDL_PROGRAMS:                value = paths.MSWGetShellDir(0x0002 | flags); break;
            case Param_CSIDL_CONTROLS:                value = paths.MSWGetShellDir(0x0003 | flags); break;
            case Param_CSIDL_PRINTERS:                value = paths.MSWGetShellDir(0x0004 | flags); break;
            case Param_CSIDL_FAVORITES:               value = paths.MSWGetShellDir(0x0006 | flags); break;
            case Param_CSIDL_STARTUP:                 value = paths.MSWGetShellDir(0x0007 | flags); break;
            case Param_CSIDL_RECENT:                  value = paths.MSWGetShellDir(0x0008 | flags); break;
            case Param_CSIDL_SENDTO:                  value = paths.MSWGetShellDir(0x0009 | flags); break;
            case Param_CSIDL_BITBUCKET:               value = paths.MSWGetShellDir(0x000a | flags); break;
            case Param_CSIDL_STARTMENU:               value = paths.MSWGetShellDir(0x000b | flags); break;
            case Param_CSIDL_MYDOCUMENTS:             value = paths.MSWGetShellDir(0x0005 | flags); break;
            case Param_CSIDL_MYMUSIC:                 value = paths.MSWGetShellDir(0x000d | flags); break;
            case Param_CSIDL_MYVIDEO:                 value = paths.MSWGetShellDir(0x000e | flags); break;
            case Param_CSIDL_DESKTOPDIRECTORY:        value = paths.MSWGetShellDir(0x0010 | flags); break;
            case Param_CSIDL_DRIVES:                  value = paths.MSWGetShellDir(0x0011 | flags); break;
            case Param_CSIDL_NETWORK:                 value = paths.MSWGetShellDir(0x0012 | flags); break;
            case Param_CSIDL_NETHOOD:                 value = paths.MSWGetShellDir(0x0013 | flags); break;
            case Param_CSIDL_FONTS:                   value = paths.MSWGetShellDir(0x0014 | flags); break;
            case Param_CSIDL_TEMPLATES:               value = paths.MSWGetShellDir(0x0015 | flags); break;
            case Param_CSIDL_COMMON_STARTMENU:        value = paths.MSWGetShellDir(0x0016 | flags); break;
            case Param_CSIDL_COMMON_PROGRAMS:         value = paths.MSWGetShellDir(0X0017 | flags); break;
            case Param_CSIDL_COMMON_STARTUP:          value = paths.MSWGetShellDir(0x0018 | flags); break;
            case Param_CSIDL_COMMON_DESKTOPDIRECTORY: value = paths.MSWGetShellDir(0x0019 | flags); break;
            case Param_CSIDL_APPDATA:                 value = paths.MSWGetShellDir(0x001a | flags); break;
            case Param_CSIDL_PRINTHOOD:               value = paths.MSWGetShellDir(0x001b | flags); break;
            case Param_CSIDL_LOCAL_APPDATA:           value = paths.MSWGetShellDir(0x001c | flags); break;
            case Param_CSIDL_ALTSTARTUP:              value = paths.MSWGetShellDir(0x001d | flags); break;
            case Param_CSIDL_COMMON_ALTSTARTUP:       value = paths.MSWGetShellDir(0x001e | flags); break;
            case Param_CSIDL_COMMON_FAVORITES:        value = paths.MSWGetShellDir(0x001f | flags); break;
            case Param_CSIDL_INTERNET_CACHE:          value = paths.MSWGetShellDir(0x0020 | flags); break;
            case Param_CSIDL_COOKIES:                 value = paths.MSWGetShellDir(0x0021 | flags); break;
            case Param_CSIDL_HISTORY:                 value = paths.MSWGetShellDir(0x0022 | flags); break;
            case Param_CSIDL_COMMON_APPDATA:          value = paths.MSWGetShellDir(0x0023 | flags); break;
            case Param_CSIDL_WINDOWS:                 value = paths.MSWGetShellDir(0x0024 | flags); break;
            case Param_CSIDL_SYSTEM:                  value = paths.MSWGetShellDir(0x0025 | flags); break;
            case Param_CSIDL_PROGRAM_FILES:           value = paths.MSWGetShellDir(0x0026 | flags); break;
            case Param_CSIDL_MYPICTURES:              value = paths.MSWGetShellDir(0x0027 | flags); break;
            case Param_CSIDL_PROFILE:                 value = paths.MSWGetShellDir(0x0028 | flags); break;
            case Param_CSIDL_SYSTEMX86:               value = paths.MSWGetShellDir(0x0029 | flags); break;
            case Param_CSIDL_PROGRAM_FILESX86:        value = paths.MSWGetShellDir(0x002a | flags); break;
            case Param_CSIDL_PROGRAM_FILES_COMMON:    value = paths.MSWGetShellDir(0x002b | flags); break;
            case Param_CSIDL_PROGRAM_FILES_COMMONX86: value = paths.MSWGetShellDir(0x002c | flags); break;
            case Param_CSIDL_COMMON_TEMPLATES:        value = paths.MSWGetShellDir(0x002d | flags); break;
            case Param_CSIDL_COMMON_DOCUMENTS:        value = paths.MSWGetShellDir(0x002e | flags); break;
            case Param_CSIDL_COMMON_ADMINTOOLS:       value = paths.MSWGetShellDir(0x002f | flags); break;
            case Param_CSIDL_ADMINTOOLS:              value = paths.MSWGetShellDir(0x0030 | flags); break;
            case Param_CSIDL_CONNECTIONS:             value = paths.MSWGetShellDir(0x0031 | flags); break;
            case Param_CSIDL_COMMON_MUSIC:            value = paths.MSWGetShellDir(0x0035 | flags); break;
            case Param_CSIDL_COMMON_PICTURES:         value = paths.MSWGetShellDir(0x0036 | flags); break;
            case Param_CSIDL_COMMON_VIDEO:            value = paths.MSWGetShellDir(0x0037 | flags); break;
            case Param_CSIDL_RESOURCES:               value = paths.MSWGetShellDir(0x0038 | flags); break;
            case Param_CSIDL_RESOURCES_LOCALIZED:     value = paths.MSWGetShellDir(0x0039 | flags); break;
            case Param_CSIDL_COMMON_OEM_LINKS:        value = paths.MSWGetShellDir(0x003a | flags); break;
            case Param_CSIDL_COMPUTERSNEARME:         value = paths.MSWGetShellDir(0x003d | flags); break;
#endif // #ifdef __WXMSW__

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

    // sort variables alphabetically by name
    std::map<wxString, wxString> variablesSorted;

    for ( auto i = variables.begin(); i != variables.end(); ++i )
        variablesSorted[i->first] = i->second;

    for ( const auto& variable : variablesSorted )
    {
         const long itemIndex = InsertItem(GetItemCount(), variable.first);

         SetItem(itemIndex, Column_Value, variable.second);
    }
}


/*************************************************

    MiscellaneousView

*************************************************/

class MiscellaneousView : public SysInfoListView
{
public:
    MiscellaneousView(wxWindow* parent);

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
        Param_ComCtl32Version,
        Param_IsProcessDPIAware,
        Param_ProcessDPIAwareness,
        Param_ThreadDPIAwarenessContext,
        Param_ProcessSystemDPI,
        Param_PathSeparator,
        Param_UserId,
        Param_UserName,
        Param_SystemEncodingName,
        Param_SystemLanguage,
        Param_HostName,
        Param_FullHostName,
        Param_OSDescription,
        Param_OSVersion,
        Param_OSDirectory,
        Param_IsPlatform64Bit,
        Param_IsPlatformLittleEndian,
    };
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
    static unsigned GetSystemDpiForThisProcess();

    static DPIAwarenessContext GetThreadDPIAwarenessContext();
    static wxString GetThreadDPIAwarenessContextStr();
};

bool MSWDPIAwarenessHelper::IsThisProcessDPIAware()
{
    typedef BOOL (WINAPI *IsProcessDPIAware_t)();

    static bool s_initialised = false;
    static IsProcessDPIAware_t s_pfnIsProcessDPIAware = nullptr;

    if ( !s_initialised )
    {
        wxDynamicLibrary dllUser32("user32.dll", wxDL_VERBATIM | wxDL_QUIET);

        wxDL_INIT_FUNC(s_pfn, IsProcessDPIAware, dllUser32);
        s_initialised = true;
    }

    if ( !s_pfnIsProcessDPIAware )
        return false;

    return s_pfnIsProcessDPIAware() == TRUE;
}

MSWDPIAwarenessHelper::ProcessDPIAwareness MSWDPIAwarenessHelper::GetThisProcessDPIAwareness()
{
    typedef HRESULT (WINAPI *GetProcessDpiAwareness_t)(HANDLE, DWORD*);

    static bool s_initialised = false;
    static wxDynamicLibrary s_dllShcore;
    static GetProcessDpiAwareness_t s_pfnGetProcessDpiAwareness = nullptr;

    if ( !s_initialised )
    {
        if ( s_dllShcore.Load("shcore.dll", wxDL_VERBATIM | wxDL_QUIET) )
            wxDL_INIT_FUNC(s_pfn, GetProcessDpiAwareness, s_dllShcore);

        if ( !s_pfnGetProcessDpiAwareness )
            s_dllShcore.Unload();

        s_initialised = true;
    }

    if ( !s_pfnGetProcessDpiAwareness )
        return ProcessDPINotApplicable;

    DWORD value = ProcessDPINotApplicable;
    const HRESULT hr = s_pfnGetProcessDpiAwareness(nullptr, &value);

    if ( FAILED(hr) )
    {
        wxLogApiError("GetProcessDpiAwareness", hr);
        return ProcessDPINotApplicable;
    }

    if ( value == ProcessDPIUnaware )
        return ProcessDPIUnaware;
    if ( value == ProcessSystemDPIAware )
        return ProcessSystemDPIAware;
    if ( value == ProcessPerMonitorDPIAware )
        return ProcessPerMonitorDPIAware;

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
    typedef unsigned (WINAPI *GetSystemDpiForProcess_t)(HANDLE);

    static bool s_initialised = false;
    static GetSystemDpiForProcess_t s_pfnGetSystemDpiForProcess = nullptr;

    if ( !s_initialised )
    {
        wxDynamicLibrary dllUser32("user32.dll", wxDL_VERBATIM | wxDL_QUIET);

        wxDL_INIT_FUNC(s_pfn, GetSystemDpiForProcess, dllUser32);
        s_initialised = true;
    }

    if ( !s_pfnGetSystemDpiForProcess )
        return 96;

    return s_pfnGetSystemDpiForProcess(nullptr);
}

MSWDPIAwarenessHelper::DPIAwarenessContext MSWDPIAwarenessHelper::GetThreadDPIAwarenessContext()
{
    typedef HANDLE MSW_DPI_AWARENESS_CONTEXT;
    static const MSW_DPI_AWARENESS_CONTEXT MSW_DPI_AWARENESS_CONTEXT_UNAWARE              = (MSW_DPI_AWARENESS_CONTEXT)-1;
    static const MSW_DPI_AWARENESS_CONTEXT MSW_DPI_AWARENESS_CONTEXT_SYSTEM_AWARE         = (MSW_DPI_AWARENESS_CONTEXT)-2;
    static const MSW_DPI_AWARENESS_CONTEXT MSW_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE    = (MSW_DPI_AWARENESS_CONTEXT)-3;
    static const MSW_DPI_AWARENESS_CONTEXT MSW_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 = (MSW_DPI_AWARENESS_CONTEXT)-4;
    static const MSW_DPI_AWARENESS_CONTEXT MSW_DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED    = (MSW_DPI_AWARENESS_CONTEXT)-5;

    typedef MSW_DPI_AWARENESS_CONTEXT (WINAPI *GetThreadDpiAwarenessContext_t)();
    typedef BOOL (WINAPI *AreDpiAwarenessContextsEqual_t)(MSW_DPI_AWARENESS_CONTEXT, MSW_DPI_AWARENESS_CONTEXT);
    typedef BOOL (WINAPI *IsValidDpiAwarenessContext_t)(MSW_DPI_AWARENESS_CONTEXT);

    static bool s_initialised = false;
    static GetThreadDpiAwarenessContext_t s_pfnGetThreadDpiAwarenessContext = nullptr;
    static AreDpiAwarenessContextsEqual_t s_pfnAreDpiAwarenessContextsEqual = nullptr;
    static IsValidDpiAwarenessContext_t   s_pfnIsValidDpiAwarenessContext   = nullptr;

    if ( !s_initialised )
    {
        wxDynamicLibrary dllUser32("user32.dll", wxDL_VERBATIM | wxDL_QUIET);

        wxDL_INIT_FUNC(s_pfn, GetThreadDpiAwarenessContext, dllUser32);
        wxDL_INIT_FUNC(s_pfn, AreDpiAwarenessContextsEqual, dllUser32);
        wxDL_INIT_FUNC(s_pfn, IsValidDpiAwarenessContext, dllUser32);
        s_initialised = true;
    }

    if ( !s_pfnGetThreadDpiAwarenessContext
         || !s_pfnAreDpiAwarenessContextsEqual
         || !s_pfnIsValidDpiAwarenessContext )
    {
        return DPIAwarenessContextNotApplicable;
    }

    const MSW_DPI_AWARENESS_CONTEXT value = s_pfnGetThreadDpiAwarenessContext();

    if ( !s_pfnIsValidDpiAwarenessContext(value) )
        return DPIAwarenessContextUnaware;

    if ( s_pfnAreDpiAwarenessContextsEqual(value, MSW_DPI_AWARENESS_CONTEXT_UNAWARE) )
        return DPIAwarenessContextUnaware;
    if ( s_pfnAreDpiAwarenessContextsEqual(value, MSW_DPI_AWARENESS_CONTEXT_SYSTEM_AWARE) )
        return DPIAwarenessContextSystemAware;
    if ( s_pfnAreDpiAwarenessContextsEqual(value, MSW_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE) )
        return DPIAwarenessContextPerMonitorAware;
    if ( s_pfnAreDpiAwarenessContextsEqual(value, MSW_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) )
        return DPIAwarenessContextPerMonitorAwareV2;
    if ( s_pfnAreDpiAwarenessContextsEqual(value, MSW_DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED) )
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

MiscellaneousView::MiscellaneousView(wxWindow* parent)
    : SysInfoListView(parent)
{
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
    AppendItemWithData(_("ComCtl32.dll Version"), Param_ComCtl32Version);
    AppendItemWithData(_("Is Process DPI Aware"), Param_IsProcessDPIAware);
    AppendItemWithData(_("Process DPI Awareness"), Param_ProcessDPIAwareness);
    AppendItemWithData(_("Thread DPI Awareness Context"), Param_ThreadDPIAwarenessContext);
    AppendItemWithData(_("System DPI for Process"), Param_ProcessSystemDPI);

#endif // #ifdef __WXMSW__
    AppendItemWithData(_("Path Separator"), Param_PathSeparator);
    AppendItemWithData(_("User Id"), Param_UserId);
    AppendItemWithData(_("User Name"), Param_UserName);
    AppendItemWithData(_("System Encoding"), Param_SystemEncodingName);
    AppendItemWithData(_("System Language"), Param_SystemLanguage);
    AppendItemWithData(_("Host Name"), Param_HostName);
    AppendItemWithData(_("Full Host Name"), Param_FullHostName);
    AppendItemWithData(_("OS Description"), Param_OSDescription);
    AppendItemWithData(_("OS Version"), Param_OSVersion);
    AppendItemWithData(_("OS Directory"), Param_OSDirectory);
    AppendItemWithData(_("64-bit Platform"), Param_IsPlatform64Bit);
    AppendItemWithData(_("Little Endian"), Param_IsPlatformLittleEndian);

    UpdateValues();
}

void MiscellaneousView::DoUpdateValues()
{
    int verMajor = 0, verMinor = 0, verMicro = 0;
    wxAppConsole* appInstance = wxAppConsole::GetInstance();
    wxAppTraits* appTraits = appInstance->GetTraits();
    const long itemCount = GetItemCount();

    wxGetOsVersion(&verMajor, &verMinor, &verMicro);

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
            case Param_ComCtl32Version:           value.Printf("%d", wxApp::GetComCtl32Version()); break;
            case Param_IsProcessDPIAware:         value = MSWDPIAwarenessHelper::IsThisProcessDPIAware() ? _("Yes") : _("No"); break;
            case Param_ProcessDPIAwareness:       value = MSWDPIAwarenessHelper::GetThisProcessDPIAwarenessStr(); break;
            case Param_ThreadDPIAwarenessContext: value = MSWDPIAwarenessHelper::GetThreadDPIAwarenessContextStr(); break;
            case Param_ProcessSystemDPI:          value.Printf("%d", MSWDPIAwarenessHelper::GetSystemDpiForThisProcess()); break;
#endif // #ifdef __WXMSW__

            case Param_PathSeparator:             value.Printf("%s", wxString(wxFileName::GetPathSeparator())); break;
            case Param_UserId:                    value = wxGetUserId(); break;
            case Param_UserName:                  value = wxGetUserName(); break;
            case Param_SystemEncodingName:        value = wxLocale::GetSystemEncodingName(); break;
            case Param_SystemLanguage:            value =  wxLocale::GetLanguageName(wxLocale::GetSystemLanguage()); break;
            case Param_HostName:                  value = wxGetHostName(); break;
            case Param_FullHostName:              value = wxGetFullHostName(); break;
            case Param_OSDescription:             value =  wxGetOsDescription(); break;
            case Param_OSVersion:                 value.Printf(_("%d.%d.%d"), verMajor, verMinor, verMicro); break;
            case Param_OSDirectory:               value = wxGetOSDirectory(); break;
            case Param_IsPlatform64Bit:           value = wxIsPlatform64Bit() ? _("Yes") : _("No"); break;
            case Param_IsPlatformLittleEndian:    value =  wxIsPlatformLittleEndian() ? _("Yes") : _("No"); break;

            default:
                wxFAIL;
        }

        SetItem(i, Column_Value, value);
    }
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

#define APPEND_DEFINE_ITEM(name, value) \
    stringizedValue = wxSTRINGIZE_T(value); \
    itemIndex = InsertItem(GetItemCount(), name); \
    SetItem(itemIndex, Column_Value, DefineValueToText(name, stringizedValue));

void PreprocessorDefinesView::DoUpdateValues()
{
    // preprocesser defines cannot be
    // changed when the application is running
    if ( GetItemCount() > 0 )
        return;

    long itemIndex;
    wxString stringizedValue;

    APPEND_DEFINE_ITEM(wxS("__cplusplus"), __cplusplus)

#ifdef _MSC_VER
    APPEND_DEFINE_ITEM(wxS("_MSC_VER"), _MSC_VER)
    APPEND_DEFINE_ITEM(wxS("_MSC_FULL_VER"), _MSC_FULL_VER)
    APPEND_DEFINE_ITEM(wxS("_MSVC_LANG"), _MSVC_LANG)
    APPEND_DEFINE_ITEM(wxS("_DEBUG"), _DEBUG)
    APPEND_DEFINE_ITEM(wxS("_DLL"), _DLL)
#endif // #ifdef _MSC_VER

#ifdef __GNUC__
    APPEND_DEFINE_ITEM(wxS("__GNUC__"), __GNUC__)
    APPEND_DEFINE_ITEM(wxS("__GNUC_MINOR__"), __GNUC_MINOR__)
    APPEND_DEFINE_ITEM(wxS("__GNUC_PATCHLEVEL__"), __GNUC_PATCHLEVEL__)
#endif // #ifdef(__GNUC__)

#ifdef __MINGW32__
    APPEND_DEFINE_ITEM(wxS("__MINGW32__"), __MINGW32__)
    APPEND_DEFINE_ITEM(wxS("__MINGW32_MAJOR_VERSION"), __MINGW32_MAJOR_VERSION)
    APPEND_DEFINE_ITEM(wxS("__MINGW32_MINOR_VERSION"), __MINGW32_MINOR_VERSION)
#endif

#ifdef __MINGW64__
    APPEND_DEFINE_ITEM(wxS("__MINGW64__"), __MINGW64__)
    APPEND_DEFINE_ITEM(wxS("__MINGW64_VERSION_MAJOR"), __MINGW64_VERSION_MAJOR)
    APPEND_DEFINE_ITEM(wxS("__MINGW64_VERSION_MINOR"), __MINGW64_VERSION_MINOR)
#endif

#ifdef __MINGW32_TOOLCHAIN__
    APPEND_DEFINE_ITEM(wxS("__MINGW32_TOOLCHAIN__"), __MINGW32_TOOLCHAIN__)
#endif

#ifdef __MINGW64_TOOLCHAIN__
    APPEND_DEFINE_ITEM(wxS("__MINGW64_TOOLCHAIN__"), __MINGW64_TOOLCHAIN__)
#endif

#ifdef __clang__
    APPEND_DEFINE_ITEM(wxS("__clang__"), __clang__)
    APPEND_DEFINE_ITEM(wxS("__clang_major__"), __clang_major__)
    APPEND_DEFINE_ITEM(wxS("__clang_minor__"), __clang_minor__)
    APPEND_DEFINE_ITEM(wxS("__clang_patchlevel__"), __clang_patchlevel__)
#endif

#ifdef __CYGWIN__
    APPEND_DEFINE_ITEM(wxS("__CYGWIN__"), __CYGWIN__)
#endif

#ifdef __WXMSW__
    APPEND_DEFINE_ITEM(wxS("WXUSINGDLL"), WXUSINGDLL)
    APPEND_DEFINE_ITEM(wxS("_UNICODE"), _UNICODE)
#endif

#ifdef __WXGTK__
    APPEND_DEFINE_ITEM(wxS("__WXGTK3__"), __WXGTK3__)
#endif

    APPEND_DEFINE_ITEM(wxS("NDEBUG"), NDEBUG)

    APPEND_DEFINE_ITEM(wxS("WXWIN_COMPATIBILITY_2_8"), WXWIN_COMPATIBILITY_2_8)
    APPEND_DEFINE_ITEM(wxS("WXWIN_COMPATIBILITY_3_0"), WXWIN_COMPATIBILITY_3_0)
    APPEND_DEFINE_ITEM(wxS("wxUSE_REPRODUCIBLE_BUILD"), wxUSE_REPRODUCIBLE_BUILD)
    APPEND_DEFINE_ITEM(wxS("wxDEBUG_LEVEL"), wxDEBUG_LEVEL)
    APPEND_DEFINE_ITEM(wxS("wxUSE_ON_FATAL_EXCEPTION"), wxUSE_ON_FATAL_EXCEPTION)
    APPEND_DEFINE_ITEM(wxS("wxUSE_STACKWALKER"), wxUSE_STACKWALKER)
    APPEND_DEFINE_ITEM(wxS("wxUSE_DEBUGREPORT"), wxUSE_DEBUGREPORT)
    APPEND_DEFINE_ITEM(wxS("wxUSE_DEBUG_CONTEXT"), wxUSE_DEBUG_CONTEXT)
    APPEND_DEFINE_ITEM(wxS("wxUSE_MEMORY_TRACING"), wxUSE_MEMORY_TRACING)
    APPEND_DEFINE_ITEM(wxS("wxUSE_GLOBAL_MEMORY_OPERATORS"), wxUSE_GLOBAL_MEMORY_OPERATORS)
    APPEND_DEFINE_ITEM(wxS("wxUSE_DEBUG_NEW_ALWAYS"), wxUSE_DEBUG_NEW_ALWAYS)
    APPEND_DEFINE_ITEM(wxS("wxUSE_UNICODE"), wxUSE_UNICODE)
    APPEND_DEFINE_ITEM(wxS("wxUSE_UNICODE_WCHAR"), wxUSE_UNICODE_WCHAR)
    APPEND_DEFINE_ITEM(wxS("wxUSE_UNICODE_UTF8"), wxUSE_UNICODE_UTF8)
    APPEND_DEFINE_ITEM(wxS("wxUSE_UTF8_LOCALE_ONLY"), wxUSE_UTF8_LOCALE_ONLY)
    APPEND_DEFINE_ITEM(wxS("wxUSE_WCHAR_T"), wxUSE_WCHAR_T)
    APPEND_DEFINE_ITEM(wxS("wxUSE_UNSAFE_WXSTRING_CONV"), wxUSE_UNSAFE_WXSTRING_CONV)
    APPEND_DEFINE_ITEM(wxS("wxNO_UNSAFE_WXSTRING_CONV"), wxNO_UNSAFE_WXSTRING_CONV)
    APPEND_DEFINE_ITEM(wxS("wxUSE_EXCEPTIONS"), wxUSE_EXCEPTIONS)
    APPEND_DEFINE_ITEM(wxS("wxUSE_EXTENDED_RTTI"), wxUSE_EXTENDED_RTTI)
    APPEND_DEFINE_ITEM(wxS("wxUSE_LOG"), wxUSE_LOG)
    APPEND_DEFINE_ITEM(wxS("wxUSE_LOGWINDOW"), wxUSE_LOGWINDOW)
    APPEND_DEFINE_ITEM(wxS("wxUSE_LOGGUI"), wxUSE_LOGGUI)
    APPEND_DEFINE_ITEM(wxS("wxUSE_LOG_DIALOG"), wxUSE_LOG_DIALOG)
    APPEND_DEFINE_ITEM(wxS("wxUSE_CMDLINE_PARSER"), wxUSE_CMDLINE_PARSER)
    APPEND_DEFINE_ITEM(wxS("wxUSE_THREADS"), wxUSE_THREADS)
    APPEND_DEFINE_ITEM(wxS("wxUSE_STREAMS"), wxUSE_STREAMS)
    APPEND_DEFINE_ITEM(wxS("wxUSE_PRINTF_POS_PARAMS"), wxUSE_PRINTF_POS_PARAMS)
    APPEND_DEFINE_ITEM(wxS("wxUSE_COMPILER_TLS"), wxUSE_COMPILER_TLS)
    APPEND_DEFINE_ITEM(wxS("wxUSE_STL"), wxUSE_STL)
    APPEND_DEFINE_ITEM(wxS("wxUSE_STD_DEFAULT"), wxUSE_STD_DEFAULT)
    APPEND_DEFINE_ITEM(wxS("wxUSE_STD_CONTAINERS_COMPATIBLY"), wxUSE_STD_CONTAINERS_COMPATIBLY)
    APPEND_DEFINE_ITEM(wxS("wxUSE_STD_CONTAINERS"), wxUSE_STD_CONTAINERS)
    APPEND_DEFINE_ITEM(wxS("wxUSE_STD_IOSTREAM"), wxUSE_STD_IOSTREAM)
    APPEND_DEFINE_ITEM(wxS("wxUSE_STD_STRING"), wxUSE_STD_STRING)
    APPEND_DEFINE_ITEM(wxS("wxUSE_STD_STRING_CONV_IN_WXSTRING"), wxUSE_STD_STRING_CONV_IN_WXSTRING)
    APPEND_DEFINE_ITEM(wxS("wxUSE_IOSTREAMH"), wxUSE_IOSTREAMH)
    APPEND_DEFINE_ITEM(wxS("wxUSE_LONGLONG"), wxUSE_LONGLONG)
    APPEND_DEFINE_ITEM(wxS("wxUSE_BASE64"), wxUSE_BASE64)
    APPEND_DEFINE_ITEM(wxS("wxUSE_CONSOLE_EVENTLOOP"), wxUSE_CONSOLE_EVENTLOOP)
    APPEND_DEFINE_ITEM(wxS("wxUSE_FILE"), wxUSE_FILE)
    APPEND_DEFINE_ITEM(wxS("wxUSE_FFILE"), wxUSE_FFILE)
    APPEND_DEFINE_ITEM(wxS("wxUSE_FSVOLUME"), wxUSE_FSVOLUME)
    APPEND_DEFINE_ITEM(wxS("wxUSE_SECRETSTORE"), wxUSE_SECRETSTORE)
    APPEND_DEFINE_ITEM(wxS("wxUSE_STDPATHS"), wxUSE_STDPATHS)
    APPEND_DEFINE_ITEM(wxS("wxUSE_TEXTBUFFER"), wxUSE_TEXTBUFFER)
    APPEND_DEFINE_ITEM(wxS("wxUSE_TEXTFILE"), wxUSE_TEXTFILE)
    APPEND_DEFINE_ITEM(wxS("wxUSE_INTL"), wxUSE_INTL)
    APPEND_DEFINE_ITEM(wxS("wxUSE_XLOCALE"), wxUSE_XLOCALE)
    APPEND_DEFINE_ITEM(wxS("wxUSE_DATETIME"), wxUSE_DATETIME)
    APPEND_DEFINE_ITEM(wxS("wxUSE_TIMER"), wxUSE_TIMER)
    APPEND_DEFINE_ITEM(wxS("wxUSE_STOPWATCH"), wxUSE_STOPWATCH)
    APPEND_DEFINE_ITEM(wxS("wxUSE_FSWATCHER"), wxUSE_FSWATCHER)
    APPEND_DEFINE_ITEM(wxS("wxUSE_CONFIG"), wxUSE_CONFIG)
    APPEND_DEFINE_ITEM(wxS("wxUSE_CONFIG_NATIVE"), wxUSE_CONFIG_NATIVE)
    APPEND_DEFINE_ITEM(wxS("wxUSE_DIALUP_MANAGER"), wxUSE_DIALUP_MANAGER)
    APPEND_DEFINE_ITEM(wxS("wxUSE_DYNLIB_CLASS"), wxUSE_DYNLIB_CLASS)
    APPEND_DEFINE_ITEM(wxS("wxUSE_DYNAMIC_LOADER"), wxUSE_DYNAMIC_LOADER)
    APPEND_DEFINE_ITEM(wxS("wxUSE_SOCKETS"), wxUSE_SOCKETS)
    APPEND_DEFINE_ITEM(wxS("wxUSE_IPV6"), wxUSE_IPV6)
    APPEND_DEFINE_ITEM(wxS("wxUSE_FILESYSTEM"), wxUSE_FILESYSTEM)
    APPEND_DEFINE_ITEM(wxS("wxUSE_FS_ZIP"), wxUSE_FS_ZIP)
    APPEND_DEFINE_ITEM(wxS("wxUSE_FS_ARCHIVE"), wxUSE_FS_ARCHIVE)
    APPEND_DEFINE_ITEM(wxS("wxUSE_FS_INET"), wxUSE_FS_INET)
    APPEND_DEFINE_ITEM(wxS("wxUSE_ARCHIVE_STREAMS"), wxUSE_ARCHIVE_STREAMS)
    APPEND_DEFINE_ITEM(wxS("wxUSE_ZIPSTREAM"), wxUSE_ZIPSTREAM)
    APPEND_DEFINE_ITEM(wxS("wxUSE_TARSTREAM"), wxUSE_TARSTREAM)
    APPEND_DEFINE_ITEM(wxS("wxUSE_ZLIB"), wxUSE_ZLIB)
    APPEND_DEFINE_ITEM(wxS("wxUSE_LIBLZMA"), wxUSE_LIBLZMA)
    APPEND_DEFINE_ITEM(wxS("wxUSE_APPLE_IEEE"), wxUSE_APPLE_IEEE)
    APPEND_DEFINE_ITEM(wxS("wxUSE_JOYSTICK"), wxUSE_JOYSTICK)
    APPEND_DEFINE_ITEM(wxS("wxUSE_FONTENUM"), wxUSE_FONTENUM)
    APPEND_DEFINE_ITEM(wxS("wxUSE_FONTMAP"), wxUSE_FONTMAP)
    APPEND_DEFINE_ITEM(wxS("wxUSE_MIMETYPE"), wxUSE_MIMETYPE)
    APPEND_DEFINE_ITEM(wxS("wxUSE_PROTOCOL"), wxUSE_PROTOCOL)
    APPEND_DEFINE_ITEM(wxS("wxUSE_PROTOCOL_FILE"), wxUSE_PROTOCOL_FILE)
    APPEND_DEFINE_ITEM(wxS("wxUSE_PROTOCOL_FTP"), wxUSE_PROTOCOL_FTP)
    APPEND_DEFINE_ITEM(wxS("wxUSE_PROTOCOL_HTTP"), wxUSE_PROTOCOL_HTTP)
    APPEND_DEFINE_ITEM(wxS("wxUSE_URL"), wxUSE_URL)
    APPEND_DEFINE_ITEM(wxS("wxUSE_URL_NATIVE"), wxUSE_URL_NATIVE)
    APPEND_DEFINE_ITEM(wxS("wxUSE_VARIANT"), wxUSE_VARIANT)
    APPEND_DEFINE_ITEM(wxS("wxUSE_ANY"), wxUSE_ANY)
    APPEND_DEFINE_ITEM(wxS("wxUSE_REGEX"), wxUSE_REGEX)
    APPEND_DEFINE_ITEM(wxS("wxUSE_SYSTEM_OPTIONS"), wxUSE_SYSTEM_OPTIONS)
    APPEND_DEFINE_ITEM(wxS("wxUSE_SOUND"), wxUSE_SOUND)
    APPEND_DEFINE_ITEM(wxS("wxUSE_MEDIACTRL"), wxUSE_MEDIACTRL)
    APPEND_DEFINE_ITEM(wxS("wxUSE_XRC"), wxUSE_XRC)
    APPEND_DEFINE_ITEM(wxS("wxUSE_XML"), wxUSE_XML)
    APPEND_DEFINE_ITEM(wxS("wxUSE_AUI"), wxUSE_AUI)
    APPEND_DEFINE_ITEM(wxS("wxUSE_RIBBON"), wxUSE_RIBBON)
    APPEND_DEFINE_ITEM(wxS("wxUSE_PROPGRID"), wxUSE_PROPGRID)
    APPEND_DEFINE_ITEM(wxS("wxUSE_STC"), wxUSE_STC)
    APPEND_DEFINE_ITEM(wxS("wxUSE_WEBVIEW"), wxUSE_WEBVIEW)
    APPEND_DEFINE_ITEM(wxS("wxUSE_GRAPHICS_CONTEXT"), wxUSE_GRAPHICS_CONTEXT)
    APPEND_DEFINE_ITEM(wxS("wxUSE_CAIRO"), wxUSE_CAIRO)
    APPEND_DEFINE_ITEM(wxS("wxUSE_CONTROLS"), wxUSE_CONTROLS)
    APPEND_DEFINE_ITEM(wxS("wxUSE_MARKUP"), wxUSE_MARKUP)
    APPEND_DEFINE_ITEM(wxS("wxUSE_POPUPWIN"), wxUSE_POPUPWIN)
    APPEND_DEFINE_ITEM(wxS("wxUSE_TIPWINDOW"), wxUSE_TIPWINDOW)
    APPEND_DEFINE_ITEM(wxS("wxUSE_ACTIVITYINDICATOR"), wxUSE_ACTIVITYINDICATOR)
    APPEND_DEFINE_ITEM(wxS("wxUSE_ANIMATIONCTRL"), wxUSE_ANIMATIONCTRL)
    APPEND_DEFINE_ITEM(wxS("wxUSE_BANNERWINDOW"), wxUSE_BANNERWINDOW)
    APPEND_DEFINE_ITEM(wxS("wxUSE_BUTTON"), wxUSE_BUTTON)
    APPEND_DEFINE_ITEM(wxS("wxUSE_BMPBUTTON"), wxUSE_BMPBUTTON)
    APPEND_DEFINE_ITEM(wxS("wxUSE_CALENDARCTRL"), wxUSE_CALENDARCTRL)
    APPEND_DEFINE_ITEM(wxS("wxUSE_CHECKBOX"), wxUSE_CHECKBOX)
    APPEND_DEFINE_ITEM(wxS("wxUSE_CHOICE"), wxUSE_CHOICE)
    APPEND_DEFINE_ITEM(wxS("wxUSE_COLLPANE"), wxUSE_COLLPANE)
    APPEND_DEFINE_ITEM(wxS("wxUSE_COLOURPICKERCTRL"), wxUSE_COLOURPICKERCTRL)
    APPEND_DEFINE_ITEM(wxS("wxUSE_COMBOBOX"), wxUSE_COMBOBOX)
    APPEND_DEFINE_ITEM(wxS("wxUSE_COMMANDLINKBUTTON"), wxUSE_COMMANDLINKBUTTON)
    APPEND_DEFINE_ITEM(wxS("wxUSE_DATAVIEWCTRL"), wxUSE_DATAVIEWCTRL)
    APPEND_DEFINE_ITEM(wxS("wxUSE_DATEPICKCTRL"), wxUSE_DATEPICKCTRL)
    APPEND_DEFINE_ITEM(wxS("wxUSE_DIRPICKERCTRL"), wxUSE_DIRPICKERCTRL)
    APPEND_DEFINE_ITEM(wxS("wxUSE_EDITABLELISTBOX"), wxUSE_EDITABLELISTBOX)
    APPEND_DEFINE_ITEM(wxS("wxUSE_FILECTRL"), wxUSE_FILECTRL)
    APPEND_DEFINE_ITEM(wxS("wxUSE_FILEPICKERCTRL"), wxUSE_FILEPICKERCTRL)
    APPEND_DEFINE_ITEM(wxS("wxUSE_FONTPICKERCTRL"), wxUSE_FONTPICKERCTRL)
    APPEND_DEFINE_ITEM(wxS("wxUSE_GAUGE"), wxUSE_GAUGE)
    APPEND_DEFINE_ITEM(wxS("wxUSE_HEADERCTRL"), wxUSE_HEADERCTRL)
    APPEND_DEFINE_ITEM(wxS("wxUSE_HYPERLINKCTRL"), wxUSE_HYPERLINKCTRL)
    APPEND_DEFINE_ITEM(wxS("wxUSE_LISTBOX"), wxUSE_LISTBOX)
    APPEND_DEFINE_ITEM(wxS("wxUSE_LISTCTRL"), wxUSE_LISTCTRL)
    APPEND_DEFINE_ITEM(wxS("wxUSE_RADIOBOX"), wxUSE_RADIOBOX)
    APPEND_DEFINE_ITEM(wxS("wxUSE_RADIOBTN"), wxUSE_RADIOBTN)
    APPEND_DEFINE_ITEM(wxS("wxUSE_RICHMSGDLG"), wxUSE_RICHMSGDLG)
    APPEND_DEFINE_ITEM(wxS("wxUSE_SCROLLBAR"), wxUSE_SCROLLBAR)
    APPEND_DEFINE_ITEM(wxS("wxUSE_SEARCHCTRL"), wxUSE_SEARCHCTRL)
    APPEND_DEFINE_ITEM(wxS("wxUSE_SLIDER"), wxUSE_SLIDER)
    APPEND_DEFINE_ITEM(wxS("wxUSE_SPINBTN"), wxUSE_SPINBTN)
    APPEND_DEFINE_ITEM(wxS("wxUSE_SPINCTRL"), wxUSE_SPINCTRL)
    APPEND_DEFINE_ITEM(wxS("wxUSE_STATBOX"), wxUSE_STATBOX)
    APPEND_DEFINE_ITEM(wxS("wxUSE_STATLINE"), wxUSE_STATLINE)
    APPEND_DEFINE_ITEM(wxS("wxUSE_STATTEXT"), wxUSE_STATTEXT)
    APPEND_DEFINE_ITEM(wxS("wxUSE_STATBMP"), wxUSE_STATBMP)
    APPEND_DEFINE_ITEM(wxS("wxUSE_TEXTCTRL"), wxUSE_TEXTCTRL)
    APPEND_DEFINE_ITEM(wxS("wxUSE_TIMEPICKCTRL"), wxUSE_TIMEPICKCTRL)
    APPEND_DEFINE_ITEM(wxS("wxUSE_TOGGLEBTN"), wxUSE_TOGGLEBTN)
    APPEND_DEFINE_ITEM(wxS("wxUSE_TREECTRL"), wxUSE_TREECTRL)
    APPEND_DEFINE_ITEM(wxS("wxUSE_TREELISTCTRL"), wxUSE_TREELISTCTRL)
    APPEND_DEFINE_ITEM(wxS("wxUSE_STATUSBAR"), wxUSE_STATUSBAR)
    APPEND_DEFINE_ITEM(wxS("wxUSE_NATIVE_STATUSBAR"), wxUSE_NATIVE_STATUSBAR)
    APPEND_DEFINE_ITEM(wxS("wxUSE_TOOLBAR"), wxUSE_TOOLBAR)
    APPEND_DEFINE_ITEM(wxS("wxUSE_TOOLBAR_NATIVE"), wxUSE_TOOLBAR_NATIVE)
    APPEND_DEFINE_ITEM(wxS("wxUSE_NOTEBOOK"), wxUSE_NOTEBOOK)
    APPEND_DEFINE_ITEM(wxS("wxUSE_LISTBOOK"), wxUSE_LISTBOOK)
    APPEND_DEFINE_ITEM(wxS("wxUSE_CHOICEBOOK"), wxUSE_CHOICEBOOK)
    APPEND_DEFINE_ITEM(wxS("wxUSE_TREEBOOK"), wxUSE_TREEBOOK)
    APPEND_DEFINE_ITEM(wxS("wxUSE_TOOLBOOK"), wxUSE_TOOLBOOK)
    APPEND_DEFINE_ITEM(wxS("wxUSE_TASKBARICON"), wxUSE_TASKBARICON)
    APPEND_DEFINE_ITEM(wxS("wxUSE_GRID"), wxUSE_GRID)
    APPEND_DEFINE_ITEM(wxS("wxUSE_MINIFRAME"), wxUSE_MINIFRAME)
    APPEND_DEFINE_ITEM(wxS("wxUSE_COMBOCTRL"), wxUSE_COMBOCTRL)
    APPEND_DEFINE_ITEM(wxS("wxUSE_ODCOMBOBOX"), wxUSE_ODCOMBOBOX)
    APPEND_DEFINE_ITEM(wxS("wxUSE_BITMAPCOMBOBOX"), wxUSE_BITMAPCOMBOBOX)
    APPEND_DEFINE_ITEM(wxS("wxUSE_REARRANGECTRL"), wxUSE_REARRANGECTRL)
    APPEND_DEFINE_ITEM(wxS("wxUSE_ADDREMOVECTRL"), wxUSE_ADDREMOVECTRL)
    APPEND_DEFINE_ITEM(wxS("wxUSE_ACCEL"), wxUSE_ACCEL)
    APPEND_DEFINE_ITEM(wxS("wxUSE_ARTPROVIDER_STD"), wxUSE_ARTPROVIDER_STD)
    APPEND_DEFINE_ITEM(wxS("wxUSE_ARTPROVIDER_TANGO"), wxUSE_ARTPROVIDER_TANGO)
    APPEND_DEFINE_ITEM(wxS("wxUSE_CARET"), wxUSE_CARET)
    APPEND_DEFINE_ITEM(wxS("wxUSE_DISPLAY"), wxUSE_DISPLAY)
    APPEND_DEFINE_ITEM(wxS("wxUSE_GEOMETRY"), wxUSE_GEOMETRY)
    APPEND_DEFINE_ITEM(wxS("wxUSE_IMAGLIST"), wxUSE_IMAGLIST)
    APPEND_DEFINE_ITEM(wxS("wxUSE_INFOBAR"), wxUSE_INFOBAR)
    APPEND_DEFINE_ITEM(wxS("wxUSE_MENUS"), wxUSE_MENUS)
    APPEND_DEFINE_ITEM(wxS("wxUSE_NOTIFICATION_MESSAGE"), wxUSE_NOTIFICATION_MESSAGE)
    APPEND_DEFINE_ITEM(wxS("wxUSE_PREFERENCES_EDITOR"), wxUSE_PREFERENCES_EDITOR)
    APPEND_DEFINE_ITEM(wxS("wxUSE_PRIVATE_FONTS"), wxUSE_PRIVATE_FONTS)
    APPEND_DEFINE_ITEM(wxS("wxUSE_RICHTOOLTIP"), wxUSE_RICHTOOLTIP)
    APPEND_DEFINE_ITEM(wxS("wxUSE_SASH"), wxUSE_SASH)
    APPEND_DEFINE_ITEM(wxS("wxUSE_SPLITTER"), wxUSE_SPLITTER)
    APPEND_DEFINE_ITEM(wxS("wxUSE_TOOLTIPS"), wxUSE_TOOLTIPS)
    APPEND_DEFINE_ITEM(wxS("wxUSE_VALIDATORS"), wxUSE_VALIDATORS)
    APPEND_DEFINE_ITEM(wxS("wxUSE_AUTOID_MANAGEMENT"), wxUSE_AUTOID_MANAGEMENT)
    APPEND_DEFINE_ITEM(wxS("wxUSE_BUSYINFO"), wxUSE_BUSYINFO)
    APPEND_DEFINE_ITEM(wxS("wxUSE_CHOICEDLG"), wxUSE_CHOICEDLG)
    APPEND_DEFINE_ITEM(wxS("wxUSE_COLOURDLG"), wxUSE_COLOURDLG)
    APPEND_DEFINE_ITEM(wxS("wxUSE_DIRDLG"), wxUSE_DIRDLG)
    APPEND_DEFINE_ITEM(wxS("wxUSE_FILEDLG"), wxUSE_FILEDLG)
    APPEND_DEFINE_ITEM(wxS("wxUSE_FINDREPLDLG"), wxUSE_FINDREPLDLG)
    APPEND_DEFINE_ITEM(wxS("wxUSE_FONTDLG"), wxUSE_FONTDLG)
    APPEND_DEFINE_ITEM(wxS("wxUSE_MSGDLG"), wxUSE_MSGDLG)
    APPEND_DEFINE_ITEM(wxS("wxUSE_PROGRESSDLG"), wxUSE_PROGRESSDLG)
    APPEND_DEFINE_ITEM(wxS("wxUSE_NATIVE_PROGRESSDLG"), wxUSE_NATIVE_PROGRESSDLG)
    APPEND_DEFINE_ITEM(wxS("wxUSE_STARTUP_TIPS"), wxUSE_STARTUP_TIPS)
    APPEND_DEFINE_ITEM(wxS("wxUSE_TEXTDLG"), wxUSE_TEXTDLG)
    APPEND_DEFINE_ITEM(wxS("wxUSE_NUMBERDLG"), wxUSE_NUMBERDLG)
    APPEND_DEFINE_ITEM(wxS("wxUSE_SPLASH"), wxUSE_SPLASH)
    APPEND_DEFINE_ITEM(wxS("wxUSE_WIZARDDLG"), wxUSE_WIZARDDLG)
    APPEND_DEFINE_ITEM(wxS("wxUSE_ABOUTDLG"), wxUSE_ABOUTDLG)
    APPEND_DEFINE_ITEM(wxS("wxUSE_FILE_HISTORY"), wxUSE_FILE_HISTORY)
    APPEND_DEFINE_ITEM(wxS("wxUSE_METAFILE"), wxUSE_METAFILE)
    APPEND_DEFINE_ITEM(wxS("wxUSE_WIN_METAFILES_ALWAYS"), wxUSE_WIN_METAFILES_ALWAYS)
    APPEND_DEFINE_ITEM(wxS("wxUSE_MDI"), wxUSE_MDI)
    APPEND_DEFINE_ITEM(wxS("wxUSE_DOC_VIEW_ARCHITECTURE"), wxUSE_DOC_VIEW_ARCHITECTURE)
    APPEND_DEFINE_ITEM(wxS("wxUSE_MDI_ARCHITECTURE"), wxUSE_MDI_ARCHITECTURE)
    APPEND_DEFINE_ITEM(wxS("wxUSE_PRINTING_ARCHITECTURE"), wxUSE_PRINTING_ARCHITECTURE)
    APPEND_DEFINE_ITEM(wxS("wxUSE_HTML"), wxUSE_HTML)
    APPEND_DEFINE_ITEM(wxS("wxUSE_GLCANVAS"), wxUSE_GLCANVAS)
    APPEND_DEFINE_ITEM(wxS("wxUSE_RICHTEXT"), wxUSE_RICHTEXT)
    APPEND_DEFINE_ITEM(wxS("wxUSE_CLIPBOARD"), wxUSE_CLIPBOARD)
    APPEND_DEFINE_ITEM(wxS("wxUSE_DATAOBJ"), wxUSE_DATAOBJ)
    APPEND_DEFINE_ITEM(wxS("wxUSE_DRAG_AND_DROP"), wxUSE_DRAG_AND_DROP)
    APPEND_DEFINE_ITEM(wxS("wxUSE_SNGLINST_CHECKER"), wxUSE_SNGLINST_CHECKER)
    APPEND_DEFINE_ITEM(wxS("wxUSE_DRAGIMAGE"), wxUSE_DRAGIMAGE)
    APPEND_DEFINE_ITEM(wxS("wxUSE_IPC"), wxUSE_IPC)
    APPEND_DEFINE_ITEM(wxS("wxUSE_HELP"), wxUSE_HELP)
    APPEND_DEFINE_ITEM(wxS("wxUSE_WXHTML_HELP"), wxUSE_WXHTML_HELP)
    APPEND_DEFINE_ITEM(wxS("wxUSE_CONSTRAINTS"), wxUSE_CONSTRAINTS)
    APPEND_DEFINE_ITEM(wxS("wxUSE_SPLINES"), wxUSE_SPLINES)
    APPEND_DEFINE_ITEM(wxS("wxUSE_MOUSEWHEEL"), wxUSE_MOUSEWHEEL)
    APPEND_DEFINE_ITEM(wxS("wxUSE_UIACTIONSIMULATOR"), wxUSE_UIACTIONSIMULATOR)
    APPEND_DEFINE_ITEM(wxS("wxUSE_POSTSCRIPT"), wxUSE_POSTSCRIPT)
    APPEND_DEFINE_ITEM(wxS("wxUSE_AFM_FOR_POSTSCRIPT"), wxUSE_AFM_FOR_POSTSCRIPT)
    APPEND_DEFINE_ITEM(wxS("wxUSE_SVG"), wxUSE_SVG)
    APPEND_DEFINE_ITEM(wxS("wxUSE_DC_TRANSFORM_MATRIX"), wxUSE_DC_TRANSFORM_MATRIX)
    APPEND_DEFINE_ITEM(wxS("wxUSE_IMAGE"), wxUSE_IMAGE)
    APPEND_DEFINE_ITEM(wxS("wxUSE_LIBPNG"), wxUSE_LIBPNG)
    APPEND_DEFINE_ITEM(wxS("wxUSE_LIBJPEG"), wxUSE_LIBJPEG)
    APPEND_DEFINE_ITEM(wxS("wxUSE_LIBTIFF"), wxUSE_LIBTIFF)
    APPEND_DEFINE_ITEM(wxS("wxUSE_TGA"), wxUSE_TGA)
    APPEND_DEFINE_ITEM(wxS("wxUSE_GIF"), wxUSE_GIF)
    APPEND_DEFINE_ITEM(wxS("wxUSE_PNM"), wxUSE_PNM)
    APPEND_DEFINE_ITEM(wxS("wxUSE_PCX"), wxUSE_PCX)
    APPEND_DEFINE_ITEM(wxS("wxUSE_IFF"), wxUSE_IFF)
    APPEND_DEFINE_ITEM(wxS("wxUSE_XPM"), wxUSE_XPM)
    APPEND_DEFINE_ITEM(wxS("wxUSE_ICO_CUR"), wxUSE_ICO_CUR)
    APPEND_DEFINE_ITEM(wxS("wxUSE_PALETTE"), wxUSE_PALETTE)
    APPEND_DEFINE_ITEM(wxS("wxUSE_WXDIB"), wxUSE_WXDIB)
    APPEND_DEFINE_ITEM(wxS("wxUSE_OWNER_DRAWN"), wxUSE_OWNER_DRAWN)
    APPEND_DEFINE_ITEM(wxS("wxUSE_TASKBARICON_BALLOONS"), wxUSE_TASKBARICON_BALLOONS)
    APPEND_DEFINE_ITEM(wxS("wxUSE_TASKBARBUTTON"), wxUSE_TASKBARBUTTON)
    APPEND_DEFINE_ITEM(wxS("wxUSE_INICONF"), wxUSE_INICONF)
    APPEND_DEFINE_ITEM(wxS("wxUSE_DATEPICKCTRL_GENERIC"), wxUSE_DATEPICKCTRL_GENERIC)
    APPEND_DEFINE_ITEM(wxS("wxUSE_TIMEPICKCTRL_GENERIC"), wxUSE_TIMEPICKCTRL_GENERIC)

#ifndef __WXMSW__
    APPEND_DEFINE_ITEM(wxS("wxUSE_WEBVIEW_WEBKIT"), wxUSE_WEBVIEW_WEBKIT)
#else
    APPEND_DEFINE_ITEM(wxS("wxUSE_ACCESSIBILITY"), wxUSE_ACCESSIBILITY)
    APPEND_DEFINE_ITEM(wxS("wxUSE_ACTIVEX"), wxUSE_ACTIVEX)
    APPEND_DEFINE_ITEM(wxS("wxUSE_COMBOCTRL_POPUP_ANIMATION"), wxUSE_COMBOCTRL_POPUP_ANIMATION)
    APPEND_DEFINE_ITEM(wxS("wxUSE_COMMON_DIALOGS"), wxUSE_COMMON_DIALOGS)
    APPEND_DEFINE_ITEM(wxS("wxUSE_CRASHREPORT"), wxUSE_CRASHREPORT)
    APPEND_DEFINE_ITEM(wxS("wxUSE_DBGHELP"), wxUSE_DBGHELP)
    APPEND_DEFINE_ITEM(wxS("wxUSE_DC_CACHEING"), wxUSE_DC_CACHEING)
    APPEND_DEFINE_ITEM(wxS("wxUSE_DDE_FOR_IPC"), wxUSE_DDE_FOR_IPC)
    APPEND_DEFINE_ITEM(wxS("wxDIALOG_UNIT_COMPATIBILITY"), wxDIALOG_UNIT_COMPATIBILITY)
    APPEND_DEFINE_ITEM(wxS("wxUSE_ENH_METAFILE"), wxUSE_ENH_METAFILE)
    APPEND_DEFINE_ITEM(wxS("wxUSE_GRAPHICS_GDIPLUS"), wxUSE_GRAPHICS_GDIPLUS)
    APPEND_DEFINE_ITEM(wxS("wxUSE_GRAPHICS_DIRECT2D"), wxUSE_GRAPHICS_DIRECT2D)
    APPEND_DEFINE_ITEM(wxS("wxUSE_HOTKEY"), wxUSE_HOTKEY)
    APPEND_DEFINE_ITEM(wxS("wxUSE_INKEDIT"), wxUSE_INKEDIT)
    APPEND_DEFINE_ITEM(wxS("wxUSE_MS_HTML_HELP"), wxUSE_MS_HTML_HELP)
    APPEND_DEFINE_ITEM(wxS("wxUSE_NO_MANIFEST"), wxUSE_NO_MANIFEST)
    APPEND_DEFINE_ITEM(wxS("wxUSE_OLE"), wxUSE_OLE)
    APPEND_DEFINE_ITEM(wxS("wxUSE_OLE_AUTOMATION"), wxUSE_OLE_AUTOMATION)
    APPEND_DEFINE_ITEM(wxS("wxUSE_OLE_CLIPBOARD"), wxUSE_OLE_CLIPBOARD)
    APPEND_DEFINE_ITEM(wxS("wxUSE_POSTSCRIPT_ARCHITECTURE_IN_MSW"), wxUSE_POSTSCRIPT_ARCHITECTURE_IN_MSW)
    APPEND_DEFINE_ITEM(wxS("wxUSE_PS_PRINTING"), wxUSE_PS_PRINTING)
    APPEND_DEFINE_ITEM(wxS("wxUSE_REGKEY"), wxUSE_REGKEY)
    APPEND_DEFINE_ITEM(wxS("wxUSE_RC_MANIFEST"), wxUSE_RC_MANIFEST)
    APPEND_DEFINE_ITEM(wxS("wxUSE_RICHEDIT"), wxUSE_RICHEDIT)
    APPEND_DEFINE_ITEM(wxS("wxUSE_RICHEDIT2"), wxUSE_RICHEDIT2)
    APPEND_DEFINE_ITEM(wxS("wxUSE_UXTHEME"), wxUSE_UXTHEME)
    APPEND_DEFINE_ITEM(wxS("wxUSE_VC_CRTDBG"), wxUSE_VC_CRTDBG)
    APPEND_DEFINE_ITEM(wxS("wxUSE_WINRT"), wxUSE_WINRT)
    APPEND_DEFINE_ITEM(wxS("wxUSE_WINSOCK2"), wxUSE_WINSOCK2)
#endif // #ifndef __WXMSW__

#ifdef __WXGTK__
    APPEND_DEFINE_ITEM(wxS("wxUSE_DETECT_SM"), wxUSE_DETECT_SM)
    APPEND_DEFINE_ITEM(wxS("wxUSE_GTKPRINT"), wxUSE_GTKPRINT)
    APPEND_DEFINE_ITEM(wxS("wxUSE_WEBVIEW_WEBKIT2"), wxUSE_WEBVIEW_WEBKIT2)
#endif // #ifdef __WXGTK__

#ifdef __UNIX__
    APPEND_DEFINE_ITEM(wxS("wxUSE_EPOLL_DISPATCHER"), wxUSE_EPOLL_DISPATCHER)
    APPEND_DEFINE_ITEM(wxS("wxUSE_SELECT_DISPATCHER"), wxUSE_SELECT_DISPATCHER)
    APPEND_DEFINE_ITEM(wxS("wxUSE_GSTREAMER"), wxUSE_GSTREAMER)
    APPEND_DEFINE_ITEM(wxS("wxUSE_LIBMSPACK"), wxUSE_LIBMSPACK)
    APPEND_DEFINE_ITEM(wxS("wxUSE_LIBSDL"), wxUSE_LIBSDL)
    APPEND_DEFINE_ITEM(wxS("wxUSE_PLUGINS"), wxUSE_PLUGINS)
    APPEND_DEFINE_ITEM(wxS("wxUSE_XTEST"), wxUSE_XTEST)
#endif // #ifdef __UNIX__

#ifdef __WXMAC__
    APPEND_DEFINE_ITEM(wxS("wxUSE_WEBKIT"), wxUSE_WEBKIT)
#endif// #ifdef __WXMAC__

#ifdef __WXUNIVERSAL__
    APPEND_DEFINE_ITEM(wxS("wxUSE_ALL_THEMES"), wxUSE_ALL_THEMES)
    APPEND_DEFINE_ITEM(wxS("wxUSE_THEME_GTK"), wxUSE_THEME_GTK)
    APPEND_DEFINE_ITEM(wxS("wxUSE_THEME_METAL"), wxUSE_THEME_METAL)
    APPEND_DEFINE_ITEM(wxS("wxUSE_THEME_MONO"), wxUSE_THEME_MONO)
    APPEND_DEFINE_ITEM(wxS("wxUSE_THEME_WIN32"), wxUSE_THEME_WIN32)
#endif // #ifdef __WXUNIVERSAL__

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
     Create(parent, wxID_ANY, "wxSystemInformationFrame", wxDefaultPosition, size, wxDEFAULT_FRAME_STYLE, createFlags);
}


bool wxSystemInformationFrame::Create(wxWindow *parent, wxWindowID id, const wxString &title,
                                      const wxPoint& pos, const wxSize& size,
                                      long frameStyle, long createFlags)
{
    if ( !wxFrame::Create(parent, id, title, pos, size, frameStyle) )
        return false;

    m_autoRefresh = createFlags & AutoRefresh;

    wxPanel* mainPanel = new wxPanel(this);
    wxBoxSizer* mainPanelSizer = new wxBoxSizer(wxVERTICAL);

    // add buttons

    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);

    wxButton* refreshButton = new wxButton(mainPanel, wxID_ANY, _("Refresh"));
    refreshButton->Bind(wxEVT_BUTTON, &wxSystemInformationFrame::OnRefresh, this);
    buttonSizer->Add(refreshButton, wxSizerFlags().Border(wxRIGHT));

    wxButton* detailsButton = new wxButton(mainPanel, wxID_ANY, _("wxSYS Colour or Font Details..."));
    detailsButton->Bind(wxEVT_BUTTON, &wxSystemInformationFrame::OnShowDetailedInformation, this);
    buttonSizer->Add(detailsButton, wxSizerFlags().Border(wxRIGHT));

    wxButton* wxInfoButton = new wxButton(mainPanel, wxID_ANY, _("wxInfoMessageBox..."));
    wxInfoButton->Bind(wxEVT_BUTTON, &wxSystemInformationFrame::OnShowwxInfoMessageBox, this);
    buttonSizer->Add(wxInfoButton, wxSizerFlags().Border(wxRIGHT));

    wxButton* saveButton = new wxButton(mainPanel, wxID_ANY, _("Save..."));
    saveButton ->Bind(wxEVT_BUTTON, &wxSystemInformationFrame::OnSave, this);
    buttonSizer->Add(saveButton , wxSizerFlags().Border(wxRIGHT));

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

    if ( createFlags & ViewSystemOptions )
        m_pages->AddPage(new SystemOptionsView(m_pages), _("System Options"));

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

    mainPanel->SetSizer(mainPanelSizer);

    detailsButton->Bind(wxEVT_UPDATE_UI, &wxSystemInformationFrame::OnUpdateUI, this);

    m_valuesUpdateTimer.SetOwner(this);
    Bind(wxEVT_TIMER, &wxSystemInformationFrame::OnUpdateValuesTimer, this);

    Bind(wxEVT_SYS_COLOUR_CHANGED, &wxSystemInformationFrame::OnSysColourChanged, this);
    Bind(wxEVT_DISPLAY_CHANGED, &wxSystemInformationFrame::OnDisplayChanged, this);

#if 0
    // add keyboard shortcuts for refreshing values
    wxAcceleratorEntry acceleratorEntries[2];
    acceleratorEntries[0].Set(wxACCEL_NORMAL, WXK_F5, refreshButton->GetId());
    acceleratorEntries[1].Set(wxACCEL_CTRL, (int)'R', refreshButton->GetId());

    wxAcceleratorTable acceleratorTable(WXSIZEOF(acceleratorEntries), acceleratorEntries);

    SetAcceleratorTable(acceleratorTable);
#endif // #if 0

    return true;
}

wxArrayString wxSystemInformationFrame::GetValues(const wxString& separator) const
{
    const size_t pageCount = m_pages->GetPageCount();

    wxArrayString values;

    for ( size_t i = 0; i < pageCount; ++i )
    {
        const SysInfoListView* view = dynamic_cast<SysInfoListView*>(m_pages->GetPage(i));
        const wxArrayString viewValues = view->GetValues(separator);

        if ( i > 0 )
            values.push_back(wxEmptyString); // separate groups of values by an empty line

        values.push_back(m_pages->GetPageText(i));
        values.push_back("----------------------------");
        values.insert(values.end(), viewValues.begin(), viewValues.end());
    }

    return values;
}

#ifdef __WXMSW__
WXLRESULT wxSystemInformationFrame::MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
{
    if ( nMsg == WM_SETTINGCHANGE )
    {
        LogInformation(wxString::Format("WM_SETTINGCHANGE received: wParam = %u, lParam =\"%s\"",
            (unsigned)wParam, lParam ? (LPCTSTR)lParam : wxS("")));
        TriggerValuesUpdate();
    }
    else
    if ( nMsg ==  0x02E0 ) // 0x02E0 = WM_DPICHANGED
    {
        LogInformation(wxString::Format("WM_DPICHANGED received: new DPI = %u x %u",
            (unsigned)LOWORD(wParam), (unsigned)HIWORD(wParam)));
        TriggerValuesUpdate();
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
    m_logCtrl->AppendText(message);
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
    const size_t pageCount = m_pages->GetPageCount();

    {
        wxBusyCursor busyCursor;

        for ( size_t i = 0; i < pageCount; ++i )
        {
            SysInfoListView* view = dynamic_cast<SysInfoListView*>(m_pages->GetPage(i));
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
    const wxArrayString values = GetValues();
    const wxString fileName = wxFileSelector(_("Choose File Name"), "", "", "",
                                             _("Text Files (*.txt)|*.txt"),
                                             wxFD_SAVE | wxFD_OVERWRITE_PROMPT, this);

    if ( fileName.empty() )
        return;

    wxTextFile textFile(fileName);

    if ( textFile.Exists() )
    {
        if ( !textFile.Open() )
            return;
    }
    else
    {
        if ( !textFile.Create() )
            return;
    }

    textFile.Clear();

    for ( const auto& value : values )
        textFile.AddLine(value);

    textFile.Write();
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
    LogInformation(_("wxSysColourChangedEvent arrived."));
    TriggerValuesUpdate();
}

void wxSystemInformationFrame::OnDisplayChanged(wxDisplayChangedEvent& event)
{
    event.Skip();
    LogInformation(_("wxDisplayChangedEvent arrived."));
    TriggerValuesUpdate();
}