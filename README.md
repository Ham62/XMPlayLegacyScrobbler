# XMPlay Legacy Scrobbler
Finally, an updated scrobbler plugin with OS compatibility as great as XMPlay itself!

Legacy Scrobbler is written from the ground up leveraging OpenSSL to allow you to scrobble your favourite tracks from whatever device you're on, even if the host has no modern browsers or updated root certificates installed!

Despite the name this plugin isn't intended exclusively for legacy systems. It also acts as a lightweight scrobbler on any modern host and has no external dependencies or additional runtimes like .NET required.

This plugin has been tested and working on various Windows versions including Windows 98, 2000, XP, 7, 10, and 11.

## Home Page:
http://grahamdowney.com/software/XMPlayDiscordStatus/xmp-discordstatus.htm

## Features
* Writen to conform to the last.fm Scrobble 2.0 API
* Update your <Now Playing> status with the current playing track in real time
* Allows seeking within a track so long as minimum time playing is met before submitting
* Uses session key authentication so your password is never stored on disk
* Cache offline scrobbles to disk to submit when network connection resumes
* Enable local file logging with configurable log size limit

## Installing
Copy `xmp-legacyscrobbler.dll` into your XMPlay directory, or any sub folder within that directory, along with the included SSL libraries `LIBEAY32.DLL` and `SSLEAY32.DLL`.

Open XMPlay and navigate to the "Plugins" settings in "Options and stuff" then add the DSP plugin "XMPlay Legacy Scrobbler". Once loaded open the plugin's configuration and log into your last.fm account. The status box will turn green with the message "Logged in" once authentication is completed.

You are now ready to start scrobbling!

## Compiling
This project was built using MSVC++ 6.0. It has not been tested against any other build tools.

* `XMP Legacy Scrobbler.dsw` is the Visual Studio workspace file for this project.
* `XMPLegacyScrobbler.mak` is the nmake build script for compiling from the command line.

You must have OpenSSL installed in your compiler's include path to build this plugin.

## License
This project is licensed under the MIT license.
Copyright (c) 2025 Graham Downey

See included LICENSE.TXT for full terms.
