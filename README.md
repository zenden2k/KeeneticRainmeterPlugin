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
Interface=ISP
```

Plugin has been tested with Keenetic Viva (KN-1910).

## Obtaining interface name (optional)

Connect to your router with **telnet**:

```
telnet 192.168.1.1
```
and run command

```
show interface
```

All interfaces will be listed.

## Multiple interfaces and routers

You can specify multiple interfaces of the same router in "Rainmeter.data" file separating them with comma:

```
[KeeneticPlugin]
Interface=ISP,Wireguard0
```
Now specify the interface of a [Measure](https://docs.rainmeter.net/manual/measures/) in the skin file.

This plugin can be used with multiple routers.
You can create multiple sections with parameters in "Rainmeter.data" file. 
Then you can use section name as Router option of a Measure in the skin file: 

```
[MeasureDownloadSpeed]
Measure=Plugin
Plugin=KeeneticRainmeterPlugin
Type=download
Interface=Wireguard0
Router=MyRouter

[MeasureUploadSpeed]
Measure=Plugin
Plugin=KeeneticRainmeterPlugin
Type=upload
Interface=Wireguard0
Router=MyRouter
```

The default value for the Router option is "KeeneticPlugin". 

## Building from sources

To build this plugin from source files you will need:

- Git https://git-scm.com/downloads
- Microsoft Visual Studio 2019 or newer (with C++ compiler) https://visualstudio.microsoft.com/downloads/
- Python 3 https://www.python.org/downloads/
- Conan 2.x (C++ package manager) https://conan.io/

## Building dependencies

Run the following commands:

```bash
conan install . -g MSBuildDeps -s arch=x86 -s build_type=Release --build=missing -s compiler.runtime=static
conan install . -g MSBuildDeps -s arch=x86_64 -s build_type=Release --build=missing -s compiler.runtime=static
  
conan install . -g MSBuildDeps -s arch=x86 -s build_type=Debug --build=missing -s compiler.runtime=static
conan install . -g MSBuildDeps -s arch=x86_64 -s build_type=Debug --build=missing -s compiler.runtime=static
```

You can now load conanbuildinfo_multi.props in your Visual Studio IDE property manager, and all configurations will be loaded at once.

Dependencies

- libcurl https://github.com/curl/curl 
- jsoncpp https://github.com/open-source-parsers/jsoncpp
- utf8 https://github.com/nemtrif/utfcpp