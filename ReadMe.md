﻿wxSystemInformationFrame
=========

Introduction
---------

wxSystemInformationFrame is a wxFrame-derived class that can be easily added to a wxWidgets application and provide a quick overview of many different OS, wxWidgets, and application settings.

While this is certainly not something needed often, perhaps once in a blue moon it can save a programmer from typing a throwaway code for inspecting various variables via logging or inside the debugger.


Requirements
---------

wxWidgets v3 or newer, a compiler supporting C++11.

Using
---------

Just add *wxSystemInformationFrame.h* and *wxSystemInformationFrame.cpp* to your project/makefile and then in your application create the `wxSystemInformationFrame`, e.g. 

```cpp
#include "wxSystemInformationFrame.h"

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
