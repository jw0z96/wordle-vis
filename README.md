# wordle-vis

a silly [tweet](https://twitter.com/jw0z_etc/status/1479449670585888771)

## Compile & Run
```
$ make
$ pulseaudio -D # if pulseaudio isn't already running (e.g: macOS)
$ ./Wordle <your audio source name> # as listed by pacmd list-sources>
```

## Dependencies
- pulseaudio
- fftw
- a terminal with wide char / emoji support
