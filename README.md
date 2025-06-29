![kmp3_5](https://github.com/user-attachments/assets/dc2560dd-7667-481a-9e74-1f7fcaaa0c6e)

### Features
- Plays most of the audio/video formats.
- Cover image.
- MPRIS D-Bus controls.
- Any playback speed (no pitch correction).
- Mouse support.

### Usage
- Play each song in the directory: `kmp3 *`, or recursively: `kmp3 **/*`.
- With no arguments, stdin with pipe can be used: `find /path -iname '*.mp3' | kmp3` or whatever the shell can do.
- Navigate with vim-like keybinds.
- `h` / `l` seek back/forward.
- `n` / `p` next/prev song.
- `/` to search.
- `9` / `0` change volume, or `(` / `)` for smaller steps.
- `t` select time: `4:20`, `40` or `60%`.
- `z` focus selected song.
- `r` / `R` cycle between repeat methods (None, Track, Playlist).
- `m` mute.
- `q` quit.
- `[` / `]` playback speed shifting fun. `\` Set original speed back.

### Install
On archlinux use aur package: `yay -S kmp3-git`.\
Or build from source using instructions below.

### Dependencies
Compiler: `clang >= 14.0` or `gcc >= 12`, `CMake >= 3.20`.\
Packages: `libavformat libavcodec libavutil libswresample`.\
Audio backends: `libpipewire-0.3 #(linux)`, `sndio #(OpenBSD)`, `coreaudio #(Mac)`.\
For mpris support: `libsystemd` or `basu`.\
For image support: `chafa glib-2.0 libswscale`.

### Build
```
cmake -S . -B build/
cmake --build build/ -j
sudo cmake --install build/
```

### Uninstall
```
sudo xargs rm < ./build/install_manifest.txt
```
