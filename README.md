# XMPlay Legacy Scrobbler
Finally, an updated scrobbler plugin with OS compatibility as great as [XMPlay](https://xmplay.com/) itself!

Legacy Scrobbler is written from the ground up leveraging modern OpenSSL to allow you to scrobble your favourite tracks from whatever device you're on, even if the host has no modern browsers or updated root certificates!

Despite the name this plugin isn't intended exclusively for legacy systems. It also acts as a lightweight scrobbler on any modern host and has no external dependencies or requirements for additional runtimes like .NET!

This plugin has been tested and working on various versions of Windows including Windows 98, 2000, XP, 7, 10, and 11.

## Home Page:
http://grahamdowney.com/software/XMPlayLegacyScrobbler/

## Features
* Writen to conform to the last.fm Scrobble 2.0 API
* Update your <Now Playing> status with the current playing track in real time
* Allows seeking within a track so long as minimum time playing is met before submitting
* Uses session key authentication so your password is never stored on disk
* Caches offline scrobbles to submit when network connection resumes
* Enable local file logging with configurable log size limit

## Screenshot
![Legacy Scrobbler Settings](http://grahamdowney.com/software/XMPlayLegacyScrobbler/Settings.png)

## Installing
Copy `xmp-legacyscrobbler.dll` into your XMPlay directory, or any sub folder within that directory, along with the included SSL libraries `LIBEAY32.DLL` and `SSLEAY32.DLL`.

1. Open XMPlay and navigate to the "Plugins" settings in "Options and stuff".
2. Add the DSP plugin "XMPlay Legacy Scrobbler" from the dropdown menu.
3. Once loaded, select the plugin and click "Config".
4. Enter your last.fm account credentials and click "Log In".
5. The status box will turn green with the message "Logged in" once authentication is completed.

You are now ready to start scrobbling!

Note: On Windows 9x you must have Internet Explorer installed to provide the required Wincrypt functions necessary for generating Last.fm request signatures.

## Compiling
This project was built using MSVC++ 6.0. It has not been tested against any other build tools.

* `XMP Legacy Scrobbler.dsw` is the Visual Studio workspace file for this project.
* `XMPLegacyScrobbler.mak` is the nmake build script for compiling from the command line.

You will need create a Last.fm API account and [request an API key](https://www.last.fm/api/account/create) in order to build this plugin from source.
 
Before compiling, rename `apikeys.txt` to `apikeys.h` and enter your Last.fm API Key and Shared Secret to their respective lines.

You must have OpenSSL installed in your compiler's include path to build this plugin.

## TODO:
* Handle caching more than 50 offline scrobbles at once.
* Refactor/clean up some error handling code

## License
This project is licensed under the MIT license.

Copyright (c) 2025-2026 Graham Downey

See included LICENSE.TXT for full terms.
