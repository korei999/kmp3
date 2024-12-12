![kmp3_3](https://github.com/user-attachments/assets/3f98f29d-ac64-4b70-92dd-1be593590616)

### Features
- Plays most of the audio/video formats.
- MPRIS D-Bus controls.
- Any playback speed (no pitch correction).

### Usage
- Play each song in the directory: `kmp3 *`, or recursively: `kmp3 **/*`.
- With no arguments, stdin with pipe can be used: `find /path -iname '*.mp3' | kmp3` or whatever the shell can do.
- Navigate with vim-like keybinds.
- Mouse controls.
- `h` / `l` seek back/forward.
- `n` / `p` next/prev song.
- `/` to search.
- `9` / `0` change volume, or `(` / `)` for smaller steps.
- `t` select time: `4:20`, `40` or `60%`.
- `z` center around currently playing song.
- `r` / `R` cycle between repeat methods (None, Track, Playlist).
- `m` mute.
- `q` quit.
- `[` / `]` playback speed shifting fun. `\` Set original speed back.

### Dependencies
`libpipewire-0.3 libavformat libavcodec libavutil libswresample`.\
To enable mpris support: `libsystemd` or `basu`.\
For image support: `chafa glib-2.0 libswscale`.

### Install
```
cmake -S . -B build/
cmake --build build/ -j
sudo cmake --install build/
```

### Uninstall
```
sudo xargs rm < ./build/install_manifest.txt
```
