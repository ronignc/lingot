/*
 * lingot, a musical instrument tuner.
 *
 * Copyright (C) 2004-2010  Ibán Cereijo Graña, Jairo Chapela Martínez.
 *
 * This file is part of lingot.
 *
 * lingot is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * lingot is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with lingot; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __LINGOT_AUDIO_H__
#define __LINGOT_AUDIO_H__

#ifdef ALSA
#include <alsa/asoundlib.h>
#endif

#ifdef JACK
#include <jack/jack.h>
#endif

typedef struct _LingotAudio LingotAudio;

struct _LingotAudio {

	int audio_system;

#ifdef ALSA
	snd_pcm_t *capture_handle;
#endif

	int dsp; // file handler.
	SAMPLE_TYPE* read_buffer;

#	ifdef JACK
	jack_port_t *jack_input_port;
	jack_client_t *jack_client;
	int nframes;
#	endif

	char error_message[100];
};

// creates an audio handler
LingotAudio* lingot_audio_new(void*);

// destroys an audio handler
void lingot_audio_destroy(LingotAudio*, void*);

// reads a new piece of signal
int lingot_audio_read(LingotAudio*, void*);

#endif
