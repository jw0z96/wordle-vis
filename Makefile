Wordle: wordle.c
	gcc -o Wordle \
		wordle.c \
		-I/opt/homebrew/Cellar/pulseaudio/14.2/include \
		-L/opt/homebrew/Cellar/pulseaudio/14.2/lib \
		-lpulse-simple \
		-lpulse \
		-I/opt/homebrew/Cellar/fftw/3.3.10/include \
		-L/opt/homebrew/Cellar/fftw/3.3.10/lib \
		-lfftw3
