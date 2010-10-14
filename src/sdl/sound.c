#include <SDL.h>
#include "../client/header/client.h"
#include "../client/sound/header/local.h"

/* Global stuff */
int snd_inited = 0;
static int dmapos = 0;
static int dmasize = 0;
static dma_t *dmabackend;

/* The callback */
static void 
sdl_audio_callback(void *data, Uint8 *stream, int length)
{
	int length1;
	int length2;
	int pos = (dmapos * (dmabackend->samplebits / 8));

	if (pos >= dmasize)
	{
		dmapos = pos = 0;
	}

	/* This can't happen! */
	if (!snd_inited)
	{
		memset(stream, '\0', length);
		return;
	}

	int tobufferend = dmasize - pos;

	if (length > tobufferend)
	{
		length1 = tobufferend;
		length2 = length - length1;
	}
	else
	{
		length1= length;
		length2 = 0;
	}

	memcpy(stream, dmabackend->buffer + pos, length1);

	/* Set new position */
	if (length2 <= 0)
	{
		dmapos += (length1 / (dmabackend->samplebits / 8));
	}
	else
	{
		memcpy(stream + length1, dmabackend->buffer, length2);
		dmapos = (length2 / (dmabackend->samplebits / 8));
	}

    if (dmapos >= dmasize)
	{
		dmapos = 0;
	}
}

qboolean
SNDDMA_Init(void)
{
	char drivername[128];
	SDL_AudioSpec desired;
	SDL_AudioSpec optained;

	/* This should never happen,
	   but this is Quake 2 ... */
	if (snd_inited)
	{
		return 1;
	}

	int sndbits = (Cvar_Get("sndbits", "16", CVAR_ARCHIVE))->value;
	int sndfreq = (Cvar_Get("s_khz", "0", CVAR_ARCHIVE))->value;
	int sndchans = (Cvar_Get("sndchannels", "2", CVAR_ARCHIVE))->value;

	if (!SDL_WasInit(SDL_INIT_AUDIO))
	{
		if (SDL_Init(SDL_INIT_AUDIO) == -1)
		{
			Com_Printf ("Couldn't init SDL audio: %s\n", SDL_GetError ());
			return 0;
		}
	}
	
	if (SDL_AudioDriverName(drivername, sizeof(drivername)) == NULL)
	{
		strcpy(drivername, "(UNKNOW)");
	}

	Com_Printf("SDL audio driver is \"%s\"\n", drivername);

	memset(&desired, '\0', sizeof(desired));
	memset(&optained, '\0', sizeof(optained));

	/* Users are stupid */
	if ((sndbits != 16) && (sndbits != 8))
	{
		sndbits = 16;
	}

	if (sndfreq == 22)
	{
		desired.freq = 22050;
	}
	else if (sndfreq == 11)
	{
		desired.freq = 11025;
	}

	desired.format = ((sndbits == 16) ? AUDIO_S16SYS : AUDIO_U8);

	if (desired.freq <= 11025)
	{
		desired.samples = 256;
	}
	else if (desired.freq <= 22050)
	{
		desired.samples = 512;
	}
	else
	{
		desired.samples = 2048;
	}

	desired.channels = sndchans;
	desired.callback = sdl_audio_callback;

	/* Okay, let's try our luck */
	if (SDL_OpenAudio(&desired, &optained) == -1)
	{
		Com_Printf("SDL_OpenAudio() failed: %s\n", SDL_GetError());
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		return 0;
	}

	/* Don't pollute the frontend dma_t */
	dmabackend = &dma;
	
	dmapos = 0;
	dmabackend->samplebits = optained.format & 0xFF;
	dmabackend->channels = optained.channels;
	dmabackend->samples = 32768;
	dmabackend->submission_chunk = 1;
	dmabackend->speed = optained.freq;
	dmasize = (dmabackend->samples * (dmabackend->samplebits / 8));
	dmabackend->buffer = calloc(1, dmasize);

	Com_Printf("Starting SDL audio callback...\n");
    SDL_PauseAudio(0);

    Com_Printf("SDL audio initialized.\n");
    snd_inited = 1;
    return 1;
}

int
SNDDMA_GetDMAPos(void)
{
	return dmapos;
}

void
SNDDMA_Shutdown(void)
{
	Com_Printf("Closing SDL audio device...\n");
    SDL_PauseAudio(1);
    SDL_CloseAudio();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    free(dmabackend->buffer);
    dmabackend->buffer = NULL;
    dmapos = dmasize = 0;
    snd_inited = 0;
    Com_Printf("SDL audio device shut down.\n");
}

/* 
 * This sends the sound to the device,
 * if the DMA isn't the device itself.
 * This shouldn't be the case on all PCI
 * and PCIe soundcards
 */
void
SNDDMA_Submit(void)
{
	SDL_UnlockAudio();
}

void
SNDDMA_BeginPainting(void)
{
	SDL_LockAudio();
}
