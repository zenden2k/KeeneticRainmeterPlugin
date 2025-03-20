# KeeneticRainmeterPlugin
A speed graph for Keenetic routers, displayed directly on your Windows desktop. 

![KeeneticRainmeterPlugin screenshot](https://i.imgur.com/UhVCydL.png)

## Installation

First, you need to install [Rainmeter](https://www.rainmeter.net/) on your computer. Then, download and install the `.rmskin` file, which is available in the [Releases](https://github.com/zenden2k/KeeneticRainmeterPlugin/releases) section.

After installation, you should specify the password for your Keenetic user in the `Rainmeter.data` file (which is located by default in the `C:\Users\user\AppData\Roaming\Rainmeter` folder).  
You can also specify the username and the URL of the router's web interface.


```ini
[KeeneticPlugin]
Login=admin
Password=qwerty1234
URL=http://192.168.1.1
Interface=ISP
```

The plugin has been tested with the Keenetic Viva (KN-1910).  

## Obtaining the Interface Name (optional)

Go to your router's Web CLI http://192.168.1.1/a and run the command:

```
show interface
```

Find the JSON key `interface-name` in the results.

Another way is to connect to your router via **telnet**:

```
telnet 192.168.1.1
```
Then, run the same command:

```
show interface
```

A list of all interfaces will be displayed.

## Multiple Interfaces and Routers  

You can specify multiple interfaces of the same router in the `Rainmeter.data` file by separating them with commas:  

```
[KeeneticPlugin]
Interface=ISP,Wireguard0
```

Next, specify the interface for a [Measure](https://docs.rainmeter.net/manual/measures/) in the skin file.  

This plugin can also be used with multiple routers.  
You can create multiple sections with parameters in the `Rainmeter.data` file.  
Then, you can use the section name as the `Router` option for a Measure in the skin file:  

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

The default value for the `Router` option is `KeeneticPlugin`. 

## Custom Command

You can use a custom command from the router's REST interface. The interface name is passed as an argument to the command.

```
[KeeneticPlugin]
Command=show/interface
DownloadField=ds_fast_rate
UploadField=us_fast_rate
DownloadDivider=1000
UploadDivider=1000
;RequestType=GET
```

You can use [JsonCpp path syntax](https://open-source-parsers.github.io/jsoncpp-docs/doxygen/class_json_1_1_path.html) in the `DownloadField` and `UploadField` options.

## Building from Sources

To build this plugin from source files, you will need:

- [Git](https://git-scm.com/downloads)  
- [Microsoft Visual Studio 2019 or newer](https://visualstudio.microsoft.com/downloads/) (with the C++ compiler)  
- [Python 3](https://www.python.org/downloads/)  
- [Conan 2.x](https://conan.io/) (C++ package manager)  


## Building Dependencies

Run the following commands:

```bash
conan install . -g MSBuildDeps -s arch=x86 -s build_type=Release --build=missing -s compiler.runtime=static
conan install . -g MSBuildDeps -s arch=x86_64 -s build_type=Release --build=missing -s compiler.runtime=static
  
conan install . -g MSBuildDeps -s arch=x86 -s build_type=Debug --build=missing -s compiler.runtime=static
conan install . -g MSBuildDeps -s arch=x86_64 -s build_type=Debug --build=missing -s compiler.runtime=static
```

You can now load `conanbuildinfo_multi.props` in your Visual Studio IDE's Property Manager, and all configurations will be loaded at once.  

Dependencies

- libcurl https://github.com/curl/curl 
- jsoncpp https://github.com/open-source-parsers/jsoncpp
- utf8 https://github.com/nemtrif/utfcpp