![kmp3_7](https://github.com/user-attachments/assets/5025a0d9-3290-4c8f-a4ae-f935a75dcf72)

### Features
- Plays most of the audio/video formats.
- Cover image.
- MPRIS D-Bus controls.
- Any playback speed (no pitch correction).
- Mouse support.

### Usage
- Play each song in the directory: `kmp3 *`, or recursively: `kmp3 **/*`.
- To shuffle, sort, or filter songs, you can use a pipe: `ls ./* | sort -R | kmp3`.
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
Compiler: `clang >= 17.0` or `gcc >= 14`, `CMake >= 3.20`.\
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
