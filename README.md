# CmdFileUtils
![Console](https://img.shields.io/badge/platform-Console-blue.svg)
[![Releases](https://img.shields.io/github/release/RadAd/CmdFileUtils.svg)](https://github.com/RadAd/CmdFileUtils/releases/latest)
[![commits-since](https://img.shields.io/github/commits-since/RadAd/CmdFileUtils/latest.svg)](commits/master)
[![Build](https://img.shields.io/appveyor/ci/RadAd/CmdFileUtils.svg)](https://ci.appveyor.com/project/RadAd/CmdFileUtils)

Command line utilties for file management

## RadDir - dir replacement
This is intended to replace the dir command of cmd.exe.

To use it add a macro `doskey dir=RadDir.exe $*` to your startup script.

Improvements include smaller header and colour output.
Colours are taken from the file [RadDir.ini](RadDir/RadDir.ini).

## RadGetUrl - url downloader

## RadSync - directory synchronisation

## RadSyncPlus - two-way directory synchronisation

# Build Instructions

## Build
`msbuild /nologo CmdFileUtils.sln /p:Configuration=Release /t:Build`
