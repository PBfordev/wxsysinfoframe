wxSystemInformationFrame
=========

Introduction
---------

wxSystemInformationFrame is a class to be easily embedded into a wxWidgets application and allowing the programmer easy inspection of many different system and application settings.

Requirements
---------

wxWidgets v3 or newer, compiler supporting C++11.

Using
---------

Just add *wxSystemInformationFrame.h* and *wxSystemInformationFrame.cpp* to your project/makefile and then in your application create the `wxSystemInformationFrame`, e.g. 

```cpp
#include <wx/wxSystemInformationFrame.h>

MainFrame::OnShowSystemInformationFrame(wxCommandEvent&)
{
    wxSystemInformationFrame* frame = new wxSystemInformationFrame(mainFrame);
    frame->Show();    
}
```

Screenshots
---------

![wxSYS Colours](screenshots/colors.png?raw=true)
![wxSYS Fonts](screenshots/fonts.png?raw=true)
![wxSYS Metrics](screenshots/metrics.png?raw=true)
![Displays](screenshots/displays.png?raw=true)
![Paths](screenshots/paths.png?raw=true)
![Options](screenshots/options.png?raw=true)
![Environment Variables](screenshots/envvars.png?raw=true)
![Miscellaneous](screenshots/misc.png?raw=true)
![Preprocessor Defines](screenshots/defines.png?raw=true)

Notes
---------

On MSW, many values are affected by settings in the application manifests, such as DPI awareness.
By C++ rules, preprocessor defines can be different in different files, so this needs to be taken into account.
By OS design, once an application starts, its system environment values cannot be affected from outside the application.

Licence
---------

[wxWidgets licence](https://github.com/wxWidgets/wxWidgets/blob/master/docs/licence.txt) 
