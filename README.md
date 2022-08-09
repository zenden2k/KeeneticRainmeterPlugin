# KeeneticRainmeterPlugin
Keenetic router's speed graph directly on your Windows desktop

![KeeneticRainmeterPlugin screenshot](https://i.imgur.com/UhVCydL.png)


## Installation

First you have to install [Rainmeter](https://www.rainmeter.net/) on your computer. Then download and install .rmskin file which is available in the [Releases](https://github.com/zenden2k/KeeneticRainmeterPlugin/releases) section.

After installation you should specify password of your Keenetic user in "Rainmeter.data" file (which is placed by default in "C:\Users\user\AppData\Roaming\Rainmeter" folder).
Also you can specify the username and URL of the router web interface.

```ini
[KeeneticPlugin]
Login=admin
Password=qwerty1234
URL=http://192.168.1.1
```

Plugin has been tested with Keenetic Viva (KN-1910).


## Building from sources

To build this plugin from source files you will need:

- Git https://git-scm.com/downloads
- Microsoft Visual Studio 2019 or newer (with C++ compiler) https://visualstudio.microsoft.com/downloads/
- Python 3 https://www.python.org/downloads/
- Conan (C++ package manager) https://conan.io/

## Building dependencies

Run the following commands:

```bash
conan install . -g visual_studio_multi -s arch=x86 -s build_type=Release --build=missing -s compiler.runtime=MT
conan install . -g visual_studio_multi -s arch=x86_64 -s build_type=Release --build=missing -s compiler.runtime=MT
  
conan install . -g visual_studio_multi -s arch=x86 -s build_type=Debug --build=missing -s compiler.runtime=MTd
conan install . -g visual_studio_multi -s arch=x86_64 -s build_type=Debug --build=missing -s compiler.runtime=MTd
```

You can now load conanbuildinfo_multi.props in your Visual Studio IDE property manager, and all configurations will be loaded at once.

Dependencies

- libcurl https://github.com/curl/curl 
- jsoncpp https://github.com/open-source-parsers/jsoncpp
- utf8 https://github.com/nemtrif/utfcpp