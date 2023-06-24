# run-zcode
Run Z-Machine story files on Unix (support for Version 6 story files,
and some strange opcodes, still not implemented).

## Install

Requirements:
- git
- [meson](https://mesonbuild.com/)
- [GLib 2.0](https://gitlab.gnome.org/GNOME/glib/)
- [Gio 2.0](https://gitlab.gnome.org/GNOME/glib/)
- [SDL 2.0](https://github.com/libsdl-org/SDL)
- [SDL_image 2.0](https://github.com/libsdl-org/SDL_image)
- [SDL_ttf 2.0](https://github.com/libsdl-org/SDL_ttf)

Git clone run-zcode
```
git clone https://github.com/adriagipas/run-zcode.git
```

Compile and install run-zcode
```
cd run-zcode
meson setup --buildtype release --prefix $(INSTALLATION_PREFIX) build
cd build
meson compile
meson install
```

## Usage

In order to run a Z-Machine story file (*example.z5*) just type
```
run-zcode example.z5
```
or for a more verbose execution
```
run-zcode -v example.z5
```

It is also possible to specify a custom configuration file (*example.conf*)
```
run-zcode -c example.conf example.z5
```

By default, the game transcript (output stream 2) is redirected to the
standard output. Using option *-T,--transcript* it is possible changed
the destination file
```
run-zcode -T transcript.txt example.z5
```
