
# Playlist-Dl
Powerful. feature rich GUI playlist downloader application for Linux Desktop.

## Features: 

- In built YouTube Playlist Search
- Allows custom selection of videos from playlist
- Allows downloading Audio only in more than 10 formats including mp3, aac, m4a, vorbis, opus, flac, and wav
- Allows downloading Video in popular MP4 and MKV containers with ability to specify video and audio download quality with intuitive UI controls.
- MultiThreaded download support
- Resume your downloads anytime.
- Included Powerful Online Video Player
- Included Online YouTube Browser with adBlock 
- Light and Dark theme support

## Install:

 `snap install playlist-dl`
 
 [![Get it from the Snap Store](https://snapcraft.io/static/images/badges/en/snap-store-black.svg)](https://snapcraft.io/playlist-dl)

## Build from source

Requires CMake 3.21 or newer and Qt 6.8 or newer with the Widgets, Network,
WebEngineWidgets and WebChannel modules.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
sudo cmake --install build
```

On a host with the KDE Qt 6 snap SDK installed (`kde-qt6-core24-sdk` and
`kf6-core24`), `scripts/dev-build.sh` builds against the exact Qt the snap
ships and `scripts/dev-run.sh` launches that build.

## Screenshot

![Playlist-dl for Linux Desktop](https://github.com/keshavbhatt/playlist-dl/blob/main/screenshots/1.png?raw=true)
![Playlist-dl for Linux Desktop](https://github.com/keshavbhatt/playlist-dl/blob/main/screenshots/2.png?raw=true)
![Playlist-dl for Linux Desktop](https://github.com/keshavbhatt/playlist-dl/blob/main/screenshots/3.png?raw=true)
![Playlist-dl for Linux Desktop](https://github.com/keshavbhatt/playlist-dl/blob/main/screenshots/4.png?raw=true)
![Playlist-dl for Linux Desktop](https://github.com/keshavbhatt/playlist-dl/blob/main/screenshots/5.png?raw=true)
![Playlist-dl for Linux Desktop](https://github.com/keshavbhatt/playlist-dl/blob/main/screenshots/6.png?raw=true)
![Playlist-dl for Linux Desktop](https://github.com/keshavbhatt/playlist-dl/blob/main/screenshots/7.png?raw=true)
