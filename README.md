### Features:
- Plays most of the audio/video formats.
- MPRIS D-Bus controls.
- Any playback speed (no pitch correction).

### Usage:
- Play each song in the directory: `kmp3 *`, or recursively: `kmp3 **/*`.
- With no arguments, stdin with pipe can be used: `find /path -iname '*.mp3' | kmp3` or whatever the shell can do.
- Navigate with vim-like keybinds.
- Some mouse controls.
- `h` / `l` seek back/forward.
- `o` / `i` next/prev song.
- `/` to search.
- `9` / `0` change volume, or `(` / `)` for smaller steps.
- `t` select time: `4:20`, `40` or `60%`.
- `z` center around currently playing song.
- `r` / `R` cycle between repeat methods (None, Track, Playlist).
- `m` mute.
- `q` quit.
- `[` / `]` playback speed shifting fun. `\` Set original speed back.

### Dependencies:
`libpipewire-0.3 libavformat libavcodec libavutil libswresample`\
To enable mpris support: `libsystemd` or `basu`

### Install:
```
cmake -S . -B build/
cmake --build build/ -j
sudo cmake --install build/
```

### Uninstall
```
sudo xargs rm < ./build/install_manifest.txt
```
