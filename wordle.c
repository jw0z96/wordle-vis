#include <stdio.h>

// wide char support, needed for certain emoji
#include <wchar.h>
#include <locale.h>

#include <unistd.h> // sleep

// To read audio inputs
#include <pulse/simple.h>
#include <pulse/error.h>

#include <fftw3.h> // to do DFTs

#include <math.h> // log10

#define SECONDS (10) // capture length (seconds)
#define SAMPLE_RATE (44100) // source sample rate
#define BUFSIZE (1024) // how many samples to read per-DFT
#define SMOOTHING (0.9f) // attenuation factor for vertical smoothing

// as defined by https://www.powerlanguage.co.uk/wordle/
#define WORDLE_COLUMNS (5)
#define WORDLE_ROWS (6)

// Try to define some frequency bins within the DFT to take each amplitude from
// The DFT is only valid up to the nyquist frequency, so we can only read from
// the lower half of the buffer.
// Try to take the mid point of WORDLE_COLUMNS equally spaced bins, with some
// scaling applied to capture a more audible range. May be better with a log()
#define DFT_SCALING (0.5)
#define DFT_MIDPOINT(X) (((BUFSIZE / 2) / WORDLE_COLUMNS) * (X + 0.5) * DFT_SCALING)
static const uint32_t aui32FreqBins[WORDLE_COLUMNS] =
{
	DFT_MIDPOINT(0),
	DFT_MIDPOINT(1),
	DFT_MIDPOINT(2),
	DFT_MIDPOINT(3),
	DFT_MIDPOINT(4),
};

typedef struct _WORDLE_FRAME
{
	float afAmplitudes[WORDLE_COLUMNS];
} WORDLE_FRAME;

static const wchar_t acPalette[3] = {
	L'⬛',
	L'🟨',
	L'🟩',
};

// Define the which emoji values correspond to which amplitudes
static const float afAmplitudeBrackets[3] =
{
	0.0f, // < black
	0.5f, // < yellow
	1.0f, // < green
};

// For amplitude X, find which bracket it falls in, to get the emoji index
#define REMAP(X) ((X < afAmplitudeBrackets[1]) ? 0 : (X < afAmplitudeBrackets[2]) ? 1 : 2)

void printWordle(const WORDLE_FRAME* psWordleFrame)
{
	for (uint8_t ui8Row = 0; ui8Row < WORDLE_ROWS; ++ui8Row)
	{
		// Row height attenuation
		float fAttenuation = (float)(1 + ui8Row) / WORDLE_ROWS;

		// Vaguely center the wordle, so we don't have to have a tiny terminal
		printf("\n\t\t");

		for (uint8_t ui8Column = 0; ui8Column < WORDLE_COLUMNS; ++ui8Column)
		{
			putwchar(
				acPalette[REMAP((fAttenuation * psWordleFrame->afAmplitudes[ui8Column]))]
			);
		}
	}

	printf("\n\n\n");
}

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("usage: ./Wordle <audio source name> # as listed by pacmd list-sources\n");
		return 1;
	}

	// Enable wide char support
	setlocale(LC_ALL, "");

	// Pulse sample spec, single channel float so it's easy for FFTW
	static const pa_sample_spec ss =
	{
		.format = PA_SAMPLE_FLOAT32LE,
		.rate = SAMPLE_RATE,
		.channels = 1 // important
	};

	// Pulse error code
	int iErrorCode;

	/* Create the recording stream */
	pa_simple *psStream = pa_simple_new(
		NULL,
		"wordle",
		PA_STREAM_RECORD,
		argv[1], // blackhole2ch: "Channel_1__Channel_2.2", mic: "Channel_1"
		"record",
		&ss,
		NULL,
		NULL,
		&iErrorCode
	);

	if (!psStream)
	{
		printf("pa_simple_new() failed: %s\n", pa_strerror(iErrorCode));
		return 1;
	}

	// Prepare FFTW buffers for generating a DFT
	double adFFTWInput[BUFSIZE];
	fftw_complex* psFFTWOutput = fftw_alloc_complex(sizeof(fftw_complex) * BUFSIZE);
	fftw_plan psFFTWPlan = fftw_plan_dft_r2c_1d(
		BUFSIZE,
		&adFFTWInput[0],
		psFFTWOutput,
		(FFTW_PATIENT | FFTW_DESTROY_INPUT)
	);

	// Buffer for the pulse read samples
	float afPulseBuffer[BUFSIZE];

	// Just WORDLE_COLUMNS amplitude values
	WORDLE_FRAME sWordleFrame = {};

	const uint32_t ui32NumReads = ((SAMPLE_RATE * SECONDS) / BUFSIZE);
	for (uint32_t ui32ReadIndex = 0; ui32ReadIndex < ui32NumReads; ++ui32ReadIndex)
	{
		// Read some audio into the buffer
		if (pa_simple_read(psStream, afPulseBuffer, sizeof(afPulseBuffer), &iErrorCode) < 0)
		{
			printf("pa_simple_read() failed: %s\n", pa_strerror(iErrorCode));
			break;
		}

		// Copy (whilst casting) our audio buffer into our FFTW input buffer
		// NOTE: single channel only, so no channel interleaving
		for (uint32_t j = 0; j < BUFSIZE; ++j)
		{
			adFFTWInput[j] = afPulseBuffer[j];
		}

		// Execute the DFT
		fftw_execute(psFFTWPlan);

		// Populate WORDLE_COLUMNS amplitudes with values from our DFT
		for (uint8_t ui8Column = 0; ui8Column < WORDLE_COLUMNS; ++ui8Column)
		{
			// Take a DFT sample for this column, according to our predefined
			// frequency bins
			const fftw_complex* sDFTSample = &psFFTWOutput[aui32FreqBins[ui8Column]];
			// Can't remember why we do this, but we do
			const float fAmplitude = log10(
				(*sDFTSample[0]) *
				(*sDFTSample[0]) +
				(*sDFTSample[1]) *
				(*sDFTSample[1])
			);

			// Attenuate the previous amplitude value, and then fmax it with the
			// new value, to implement smoothing
			sWordleFrame.afAmplitudes[ui8Column] = fmax(
				(sWordleFrame.afAmplitudes[ui8Column] * SMOOTHING),
				fAmplitude
			);
		}

		// Clear the terminal
		printf("\33[2J");

		// Print our WORDLE_COLUMNS amplitudes as a Wordle graph
		printWordle(&sWordleFrame);
	}

	pa_simple_free(psStream);

	fftw_free(psFFTWOutput);
	fftw_destroy_plan(psFFTWPlan);

	return 0;
}
