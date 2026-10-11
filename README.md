# Paint 3D for Windows

A native 64-bit Windows application reproducing Microsoft Paint 3D functionality.

## Features
- **Brushes**: Marker, Calligraphy, Airbrush, Oil Brush, Watercolor, Pixel Pen, Pencil, Eraser, Flood Fill.
- **2D Shapes & Stickers**: Lines, Rectangles, Circles, Stars, Smiley faces, Heart stickers.
- **3D Primitives**: Cubes, Spheres, Cylinders, Cones, Toruses with real-time 3D rotation and directional lighting.
- **Text & Color Palette**: Text insertion tool and custom color picker (`ChooseColor`).
- **File Export**: Save 2D drawings as `.BMP`/`.PNG` and export 3D models as Wavefront `.OBJ`.

## Build Instructions (Linux / Cross-compilation)

To build `Paint3D.exe` and `Paint3D.msi`:

```bash
make
```

### Prerequisites
- `x86_64-w64-mingw32-gcc` (MinGW-w64 compiler)
- `wixl` (msitools)
- `python3` with `Pillow` (for icon generation)

To clean build artifacts:
```bash
make clean
```
