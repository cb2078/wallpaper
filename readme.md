**dependencies**: `ffmpeg.exe` in your `PATH`, visual studio build tools

**compile**: open an x64 Visual Studio command prompt, cd into the working directory and run `build`

```
>.\main.exe
usage
  attractor image [-colour-preview <int>] [common options]
  attractor video [-coefficient <string>] [-duration <int>] [-end <float>] [-fps <int>] 
    [-lossless <int>] [-start <float>] [common options]

image options
  -colour-preview <int>  make preview of a fractal in all colours, conflicts with -preview

video options
  -coefficient <string>  coefficient to change during the video, must have regex "[xy]\d"
  -duration <int>        duration in seconds, default: 15, conflicts with -preview
  -end <float>           end value for coefficient
  -fps <int>             default: 24, conflicts with -preview
  -lossless <int>        enable lossless video compression, default: 0
  -start <float>         start value for coefficient

common options
  -border <float>              default: 0.050
  -colour <colour enum>        how to colour the attractor, conflicts with -colour
  -downscale <int>             downscale from an image <downscale> times larger, default: 1
  -height <int>                default: 720
  -intensity <float>           how bright the iterations make each pixel, default: 50.000
  -light <int>                 render in light mode, default: 0
  -params <string>             file containing parameters, conflicts with -preview
  -preview <int>               show grid of some thumbnails
  -quality <int>               how many iterations to do per pixel, default: 25
  -stretch <int>               weather to stretch the fractal, default: 0
  -type <attractor type enum>  default: POLY
  -width <int>                 default: 1280

enums
  <colour enum>          KIN | INF | BLA | VID | PLA | BW | HSV | HSL | RGB | MIX
  <attractor type enum>  POLY | TRIG | SAW | TRI
```

<p align="center">
<img src="example.png"/>
</p>
