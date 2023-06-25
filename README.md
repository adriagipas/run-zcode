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
- [Fontconfig](https://gitlab.freedesktop.org/fontconfig/fontconfig)

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

It is also possible to extract the frontispiece (cover art) from a
zblorb file using the option *-C,--cover*. For example, it could be
used to generate thumbnails with thumbnailer like this:
```
[Thumbnailer Entry]
Version=1.0
Encoding=UTF-8
Type=X-Thumbnailer
Name=zblorb Thumbnailer
MimeType=application/x-zblorb;
Exec=sh -c '/home/USER/bin/zmachinethumbs.sh "%i" "%o" %s'
```
where *zmachinethumbs.sh* is:
```
if [ $# != 3 ]; then
  echo "$0: input_file_name output_file_name size"
  exit 1
fi
 
INPUT_FILE="$1"
OUTPUT_FILE="$2"
SIZE=$3

if TEMP=$(mktemp --directory --tmpdir tumbler-zblorb-XXXXXX); then
    if /home/USER/bin/run-zcode "$INPUT_FILE" -C "$TEMP/out"; then
        convert -thumbnail "$SIZE" "$TEMP/out" "$OUTPUT_FILE" 2>/dev/null	
    fi
    rm -rf $TEMP
fi
```

## Configuration file

A default configuration file looks like this
```
[Screen]
lines=25
width=80
fullscreen=false

[Fonts]
size=8
normal-roman=sans
normal-bold=sans:style=bold
normal-italic=sans:style=italic
normal-bold-italic=sans:bold:italic
fpitch-roman=mono
fpitch-bold=mono:style=bold
fpitch-italic=mono:style=italic
fpitch-bold-italic=mono:bold:italic
```

**Screen** options control the window shape:
- *lines*: number of lines
- *width*: number of monospace characters
- *fullscreen*: run story file in full screen mode.

**Fonts** options control fonts shape:
- *size*: font size
- *normal-*: a regular font
- *fpitch-*: a fixed pitch font
