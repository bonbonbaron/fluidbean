#include "fluid_midi.h"
#include "fluid_sys.h"
#include "fluid_synth.h"
#include "fluid_settings.h"


static int fluid_midi_event_length (unsigned char event);
static int fluid_isasciistring (char *s);
static long fluid_getlength (const unsigned char *s);


/* Read the entire contents of a file into memory, allocating enough memory
 * for the file, and returning the length and the buffer.
 * Note: This rewinds the file to the start before reading.
 * Returns NULL if there was an error reading or allocating memory.
 */
#define READ_FULL_INITIAL_BUFLEN 1024

static int fluid_track_reset (fluid_track_t * track);

static int fluid_player_callback (void *data, unsigned int msec);
static int fluid_player_reset (fluid_player_t * player);
static void fluid_player_update_tempo (fluid_player_t * player);

/*
 * fluid_track_send_events
 */
static void fluid_track_send_events (fluid_track_t * track, fluid_synth_t * synth, fluid_player_t * player, unsigned int ticks, int seek_ticks) {
	fluid_midi_event_t *event;
	int seeking = seek_ticks >= 0;

	if (seeking) {
		ticks = seek_ticks;					/* update target ticks */

		if (track->ticks > ticks) {
			fluid_track_reset (track);	/* reset track if seeking backwards */
		}
	}

	while (1) {

		event = track->cur;

		if (event == NULL) {
			return;
		}

		if (track->ticks + event->dtime > ticks) {
			return;
		}

		track->ticks += event->dtime;

		if (!player || event->type == MIDI_EOT) {
			/* don't send EOT events to the callback */
		} else if (seeking && track->ticks != ticks
							 && (event->type == NOTE_ON || event->type == NOTE_OFF)) {
			/* skip on/off messages */
		} else {
			if (player->playback_callback) {
				player->playback_callback (player->playback_userdata, event);
				if (event->type == NOTE_ON && event->param2 != 0
						&& !player->channel_isplaying[event->channel]) {
					player->channel_isplaying[event->channel] = TRUE;
				}
			}
		}

		if (event->type == MIDI_SET_TEMPO && player != NULL) {
			/* memorize the tempo change value coming from the MIDI file */
			fluid_atomic_int_set (&player->miditempo, event->param1);
			fluid_player_update_tempo (player);
		}

		fluid_track_next_event (track);

	}
}

/******************************************************
 *
 *     fluid_player
 */
static void
fluid_player_handle_reset_synth (void *data, const char *name, int value) {
	fluid_player_t *player = data;
	fluid_return_if_fail (player != NULL);

	player->reset_synth_between_songs = value;
}

/**
 * Create a new MIDI player.
 * @param synth Fluid synthesizer instance to create player for
 * @return New MIDI player instance or NULL on error (out of memory)
 */
fluid_player_t *new_fluid_player (fluid_synth_t * synth) {
	int i;
	fluid_player_t *player;
	player = FLUID_NEW (fluid_player_t);

	if (player == NULL) {
		return NULL;
	}

	fluid_atomic_int_set (&player->status, FLUID_PLAYER_READY);
	fluid_atomic_int_set (&player->stopping, 0);
	player->loop = 1;
	player->ntracks = 0;

	for (i = 0; i < MAX_NUMBER_OF_TRACKS; i++) {
		player->track[i] = NULL;
	}

	player->synth = synth;
	player->system_timer = NULL;
	player->sample_timer = NULL;
	player->playlist = NULL;
	player->currentfile = NULL;
	player->division = 0;

	/* internal tempo (from MIDI file) in micro seconds per quarter note */
	player->sync_mode = 1;				/* the player follows internal tempo change */
	player->miditempo = 500000;
	/* external tempo in micro seconds per quarter note */
	player->exttempo = 500000;
	/* tempo multiplier */
	player->multempo = 1.0F;

	player->deltatime = 4.0;
	player->cur_msec = 0;
	player->cur_ticks = 0;
	player->last_callback_ticks = -1;
	fluid_atomic_int_set (&player->seek_ticks, -1);
	fluid_player_set_playback_callback (player, fluid_synth_handle_midi_event,
																			synth);
	fluid_player_set_tick_callback (player, NULL, NULL);
	player->use_system_timer = fluid_settings_str_equal (synth->settings,
																											 "player.timing-source",
																											 "system");
	if (player->use_system_timer) {
		player->system_timer = new_fluid_timer ((int) player->deltatime,
																						fluid_player_callback, player,
																						TRUE, FALSE, TRUE);

		if (player->system_timer == NULL) {
			goto err;
		}
	} else {
		player->sample_timer = new_fluid_sample_timer (player->synth,
																									 fluid_player_callback,
																									 player);

		if (player->sample_timer == NULL) {
			goto err;
		}
	}

	fluid_settings_getint (synth->settings, "player.reset-synth", &i);
	fluid_player_handle_reset_synth (player, NULL, i);

	fluid_settings_callback_int (synth->settings, "player.reset-synth",
															 fluid_player_handle_reset_synth, player);

	return player;

err:
	delete_fluid_player (player);
	return NULL;
}

/**
 * Delete a MIDI player instance.
 * @param player MIDI player instance
 * @warning Do not call when the synthesizer associated to this \p player renders audio,
 * i.e. an audio driver is running or any other synthesizer thread concurrently calls
 * fluid_synth_process() or fluid_synth_nwrite_float() or fluid_synth_write_*() !
 */
void delete_fluid_player (fluid_player_t * player) {
	fluid_list_t *q;
	fluid_playlist_item *pi;

	fluid_return_if_fail (player != NULL);

	fluid_settings_callback_int (player->synth->settings, "player.reset-synth",
															 NULL, NULL);

	fluid_player_stop (player);
	fluid_player_reset (player);

	delete_fluid_timer (player->system_timer);
	delete_fluid_sample_timer (player->synth, player->sample_timer);

	while (player->playlist != NULL) {
		q = player->playlist->next;
		pi = (fluid_playlist_item *) player->playlist->data;
		FLUID_FREE (pi->filename);
		FLUID_FREE (pi->buffer);
		FLUID_FREE (pi);
		delete1_fluid_list (player->playlist);
		player->playlist = q;
	}

	FLUID_FREE (player);
}

/**
 * Registers settings related to the MIDI player
 */
void fluid_player_settings (fluid_settings_t * settings) {
	/* player.timing-source can be either "system" (use system timer)
	   or "sample" (use timer based on number of written samples) */
	fluid_settings_register_str (settings, "player.timing-source", "sample", 0);
	fluid_settings_add_option (settings, "player.timing-source", "sample");
	fluid_settings_add_option (settings, "player.timing-source", "system");

	/* Selects whether the player should reset the synth between songs, or not. */
	fluid_settings_register_int (settings, "player.reset-synth", 1, 0, 1,
															 FLUID_HINT_TOGGLED);
}


int fluid_player_reset (fluid_player_t * player) {
	int i;

	for (i = 0; i < MAX_NUMBER_OF_TRACKS; i++) {
		if (player->track[i] != NULL) {
			delete_fluid_track (player->track[i]);
			player->track[i] = NULL;
		}
	}

	for (i = 0; i < MAX_NUMBER_OF_CHANNELS; i++) {
		player->channel_isplaying[i] = FALSE;
	}

	/*    player->current_file = NULL; */
	/*    player->status = FLUID_PLAYER_READY; */
	/*    player->loop = 1; */
	player->ntracks = 0;
	player->division = 0;
	player->miditempo = 500000;
	player->deltatime = 4.0;
	return 0;
}

/*
 * fluid_player_add_track
 */
int fluid_player_add_track (fluid_player_t * player, fluid_track_t * track) {
	if (player->ntracks < MAX_NUMBER_OF_TRACKS) {
		player->track[player->ntracks++] = track;
		return FLUID_OK;
	} else {
		return FLUID_FAILED;
	}
}

/**
 * Change the MIDI callback function.
 *
 * @param player MIDI player instance
 * @param handler Pointer to callback function
 * @param handler_data Parameter sent to the callback function
 * @returns FLUID_OK
 *
 * This is usually set to fluid_synth_handle_midi_event(), but can optionally
 * be changed to a user-defined function instead, for intercepting all MIDI
 * messages sent to the synth. You can also use a midi router as the callback
 * function to modify the MIDI messages before sending them to the synth.
 *
 * @since 1.1.4
 */
int
fluid_player_set_playback_callback (fluid_player_t * player,
																		handle_midi_event_func_t handler,
																		void *handler_data) {
	player->playback_callback = handler;
	player->playback_userdata = handler_data;
	return FLUID_OK;
}

/**
 * Add a listener function for every MIDI tick change.
 *
 * @param player MIDI player instance
 * @param handler Pointer to callback function
 * @param handler_data Opaque parameter to be sent to the callback function
 * @returns #FLUID_OK
 *
 * This callback is not set by default, but can optionally
 * be changed to a user-defined function for intercepting all MIDI
 * tick changes and react to them with precision.
 *
 * @since 2.2.0
 */
int
fluid_player_set_tick_callback (fluid_player_t * player,
																handle_midi_tick_func_t handler,
																void *handler_data) {
	player->tick_callback = handler;
	player->tick_userdata = handler_data;
	return FLUID_OK;
}

/**
 * Add a MIDI file to a player queue.
 * @param player MIDI player instance
 * @param midifile File name of the MIDI file to add
 * @return #FLUID_OK or #FLUID_FAILED
 */
int fluid_player_add (fluid_player_t * player, const char *midifile) {
	fluid_playlist_item *pi = FLUID_MALLOC (sizeof (fluid_playlist_item));
	char *f = FLUID_STRDUP (midifile);

	if (!pi || !f) {
		FLUID_FREE (pi);
		FLUID_FREE (f);
		return FLUID_FAILED;
	}

	pi->filename = f;
	pi->buffer = NULL;
	pi->buffer_len = 0;
	player->playlist = fluid_list_append (player->playlist, pi);
	return FLUID_OK;
}

/**
 * Add a MIDI file to a player queue, from a buffer in memory.
 * @param player MIDI player instance
 * @param buffer Pointer to memory containing the bytes of a complete MIDI
 *   file. The data is copied, so the caller may free or modify it immediately
 *   without affecting the playlist.
 * @param len Length of the buffer, in bytes.
 * @return #FLUID_OK or #FLUID_FAILED
 */
int
fluid_player_add_mem (fluid_player_t * player, const void *buffer,
											size_t len) {
	/* Take a copy of the buffer, so the caller can free immediately. */
	fluid_playlist_item *pi = FLUID_MALLOC (sizeof (fluid_playlist_item));
	void *buf_copy = FLUID_MALLOC (len);

	if (!pi || !buf_copy) {
		FLUID_FREE (pi);
		FLUID_FREE (buf_copy);
		return FLUID_FAILED;
	}

	FLUID_MEMCPY (buf_copy, buffer, len);
	pi->filename = NULL;
	pi->buffer = buf_copy;
	pi->buffer_len = len;
	player->playlist = fluid_list_append (player->playlist, pi);
	return FLUID_OK;
}

/*
 * fluid_player_load
 */
int fluid_player_load (fluid_player_t * player, fluid_playlist_item * item) {
	fluid_midi_file *midifile;
	char *buffer;
	size_t buffer_length;
	int buffer_owned;

	if (item->filename != NULL) {
		fluid_file fp;
		/* This file is specified by filename; load the file from disk */
		/* Read the entire contents of the file into the buffer */
		fp = FLUID_FOPEN (item->filename, "rb");

		if (fp == NULL) {
			return FLUID_FAILED;
		}

		buffer = fluid_file_read_full (fp, &buffer_length);

		FLUID_FCLOSE (fp);

		if (buffer == NULL) {
			return FLUID_FAILED;
		}

		buffer_owned = 1;
	} else {
		/* This file is specified by a pre-loaded buffer; load from memory */
		buffer = (char *) item->buffer;
		buffer_length = item->buffer_len;
		/* Do not free the buffer (it is owned by the playlist) */
		buffer_owned = 0;
	}

	midifile = new_fluid_midi_file (buffer, buffer_length);

	if (midifile == NULL) {
		if (buffer_owned) {
			FLUID_FREE (buffer);
		}

		return FLUID_FAILED;
	}

	player->division = fluid_midi_file_get_division (midifile);
	fluid_player_update_tempo (player);	// Update deltatime

	if (fluid_midi_file_load_tracks (midifile, player) != FLUID_OK) {
		if (buffer_owned) {
			FLUID_FREE (buffer);
		}

		delete_fluid_midi_file (midifile);
		return FLUID_FAILED;
	}

	delete_fluid_midi_file (midifile);

	if (buffer_owned) {
		FLUID_FREE (buffer);
	}

	return FLUID_OK;
}

void fluid_player_advancefile (fluid_player_t * player) {
	if (player->playlist == NULL) {
		return;											/* No files to play */
	}

	if (player->currentfile != NULL) {
		player->currentfile = fluid_list_next (player->currentfile);
	}

	if (player->currentfile == NULL) {
		if (player->loop == 0) {
			return;										/* We're done playing */
		}

		if (player->loop > 0) {
			player->loop--;
		}

		player->currentfile = player->playlist;
	}
}

void fluid_player_playlist_load (fluid_player_t * player, unsigned int msec) {
	fluid_playlist_item *current_playitem;
	int i;

	do {
		fluid_player_advancefile (player);

		if (player->currentfile == NULL) {
			/* Failed to find next song, probably since we're finished */
			fluid_atomic_int_set (&player->status, FLUID_PLAYER_DONE);
			return;
		}

		fluid_player_reset (player);
		current_playitem = (fluid_playlist_item *) player->currentfile->data;
	}
	while (fluid_player_load (player, current_playitem) != FLUID_OK);

	/* Successfully loaded midi file */

	player->begin_msec = msec;
	player->start_msec = msec;
	player->start_ticks = 0;
	player->cur_ticks = 0;

	for (i = 0; i < player->ntracks; i++) {
		if (player->track[i] != NULL) {
			fluid_track_reset (player->track[i]);
		}
	}
}

/*
 * fluid_player_callback
 */
int fluid_player_callback (void *data, unsigned int msec) {
	int i;
	int loadnextfile;
	int status = FLUID_PLAYER_DONE;
	fluid_midi_event_t mute_event;
	fluid_player_t *player;
	fluid_synth_t *synth;
	player = (fluid_player_t *) data;
	synth = player->synth;

	loadnextfile = player->currentfile == NULL ? 1 : 0;

	fluid_midi_event_set_type (&mute_event, CONTROL_CHANGE);
	mute_event.param1 = ALL_SOUND_OFF;
	mute_event.param2 = 1;

	if (fluid_player_get_status (player) != FLUID_PLAYER_PLAYING) {
		if (fluid_atomic_int_get (&player->stopping)) {
			for (i = 0; i < synth->midi_channels; i++) {
				if (player->channel_isplaying[i]) {
					fluid_midi_event_set_channel (&mute_event, i);
					player->playback_callback (player->playback_userdata, &mute_event);
				}
			}
			fluid_atomic_int_set (&player->stopping, 0);
		}
		return 1;
	}
	do {
		float deltatime;
		int seek_ticks;

		if (loadnextfile) {
			loadnextfile = 0;
			fluid_player_playlist_load (player, msec);

			if (player->currentfile == NULL) {
				return 0;
			}
		}

		player->cur_msec = msec;
		deltatime = fluid_atomic_float_get (&player->deltatime);
		player->cur_ticks = (player->start_ticks
												 +
												 (int) ((double)
																(player->cur_msec - player->start_msec)
																/ deltatime + 0.5));	/* 0.5 to average overall error when casting */

		seek_ticks = fluid_atomic_int_get (&player->seek_ticks);
		if (seek_ticks >= 0) {
			for (i = 0; i < synth->midi_channels; i++) {
				if (player->channel_isplaying[i]) {
					fluid_midi_event_set_channel (&mute_event, i);
					player->playback_callback (player->playback_userdata, &mute_event);
				}
			}
		}

		for (i = 0; i < player->ntracks; i++) {
			if (!fluid_track_eot (player->track[i])) {
				status = FLUID_PLAYER_PLAYING;
				fluid_track_send_events (player->track[i], synth, player,
																 player->cur_ticks, seek_ticks);
			}
		}

		if (seek_ticks >= 0) {
			player->start_ticks = seek_ticks;	/* tick position of last tempo value (which is now) */
			player->cur_ticks = seek_ticks;
			player->begin_msec = msec;	/* only used to calculate the duration of playing */
			player->start_msec = msec;	/* should be the (synth)-time of the last tempo change */
			fluid_atomic_int_set (&player->seek_ticks, -1);	/* clear seek_ticks */
		}

		if (status == FLUID_PLAYER_DONE) {

			if (player->reset_synth_between_songs) {
				fluid_synth_system_reset (player->synth);
			}

			loadnextfile = 1;
		}

		if (player->tick_callback != NULL
				&& player->last_callback_ticks != player->cur_ticks) {
			player->tick_callback (player->tick_userdata, player->cur_ticks);
			player->last_callback_ticks = player->cur_ticks;
		}
	}
	while (loadnextfile);

	/* do not update the status if the player has been stopped already */
	fluid_atomic_int_compare_and_exchange (&player->status,
																				 FLUID_PLAYER_PLAYING, status);

	return 1;
}

/**
 * Activates play mode for a MIDI player if not already playing.
 * @param player MIDI player instance
 * @return #FLUID_OK on success, #FLUID_FAILED otherwise
 *
 * If the list of files added to the player has completed its requested number of loops,
 * the playlist will be restarted from the beginning with a loop count of 1.
 */
int fluid_player_play (fluid_player_t * player) {
	if (fluid_player_get_status (player) == FLUID_PLAYER_PLAYING ||
			player->playlist == NULL) {
		return FLUID_OK;
	}

	if (!player->use_system_timer) {
		fluid_sample_timer_reset (player->synth, player->sample_timer);
	}

	/* If we're at the end of the playlist and there are no loops left, loop once */
	if (player->currentfile == NULL && player->loop == 0) {
		player->loop = 1;
	}

	fluid_atomic_int_set (&player->status, FLUID_PLAYER_PLAYING);

	return FLUID_OK;
}

/**
 * Pauses the MIDI playback.
 *
 * @param player MIDI player instance
 * @return Always returns #FLUID_OK
 *
 * It will not rewind to the beginning of the file, use fluid_player_seek() for this purpose.
 */
int fluid_player_stop (fluid_player_t * player) {
	fluid_atomic_int_set (&player->status, FLUID_PLAYER_DONE);
	fluid_atomic_int_set (&player->stopping, 1);
	fluid_player_seek (player, fluid_player_get_current_tick (player));
	return FLUID_OK;
}

/**
 * Get MIDI player status.
 * @param player MIDI player instance
 * @return Player status (#fluid_player_status)
 * @since 1.1.0
 */
int fluid_player_get_status (fluid_player_t * player) {
	return fluid_atomic_int_get (&player->status);
}

/**
 * Seek in the currently playing file.
 *
 * @param player MIDI player instance
 * @param ticks the position to seek to in the current file
 * @return #FLUID_FAILED if ticks is negative or after the latest tick of the file
 * [or, since 2.1.3, if another seek operation is currently in progress],
 * #FLUID_OK otherwise.
 *
 * The actual seek will be performed when the synth calls back the player (i.e. a few
 * levels above the player's callback set with fluid_player_set_playback_callback()).
 * If the player's status is #FLUID_PLAYER_PLAYING and a previous seek operation has
 * not been completed yet, #FLUID_FAILED is returned.
 *
 * @since 2.0.0
 */
int fluid_player_seek (fluid_player_t * player, int ticks) {
	if (ticks < 0
			|| (fluid_player_get_status (player) != FLUID_PLAYER_READY
					&& ticks > fluid_player_get_total_ticks (player))) {
		return FLUID_FAILED;
	}

	if (fluid_player_get_status (player) == FLUID_PLAYER_PLAYING) {
		if (fluid_atomic_int_compare_and_exchange
				(&player->seek_ticks, -1, ticks)) {
			// new seek position has been set, as no previous seek was in progress
			return FLUID_OK;
		}
	} else {
		// If the player is not currently playing, a new seek position can be set at any time. This allows
		// the user to do:
		// fluid_player_stop();
		// fluid_player_seek(0); // to beginning
		fluid_atomic_int_set (&player->seek_ticks, ticks);
		return FLUID_OK;
	}

	// a previous seek is still in progress or hasn't been processed yet
	return FLUID_FAILED;
}


/**
 * Enable looping of a MIDI player
 *
 * @param player MIDI player instance
 * @param loop Times left to loop the playlist. -1 means loop infinitely.
 * @return Always returns #FLUID_OK
 *
 * For example, if you want to loop the playlist twice, set loop to 2
 * and call this function before you start the player.
 *
 * @since 1.1.0
 */
int fluid_player_set_loop (fluid_player_t * player, int loop) {
	player->loop = loop;
	return FLUID_OK;
}

/**
 * update the MIDI player internal deltatime dependent of actual tempo.
 * @param player MIDI player instance
 */
static void fluid_player_update_tempo (fluid_player_t * player) {
	int tempo;										/* tempo in micro seconds by quarter note */
	float deltatime;

	if (fluid_atomic_int_get (&player->sync_mode)) {
		/* take internal tempo from MIDI file */
		tempo = fluid_atomic_int_get (&player->miditempo);
		/* compute deltattime (in ms) from current tempo and apply tempo multiplier */
		deltatime = (float) tempo / (float) player->division / (float) 1000.0;
		deltatime /= fluid_atomic_float_get (&player->multempo);	/* multiply tempo */
	} else {
		/* take  external tempo */
		tempo = fluid_atomic_int_get (&player->exttempo);
		/* compute deltattime (in ms) from current tempo */
		deltatime = (float) tempo / (float) player->division / (float) 1000.0;
	}

	fluid_atomic_float_set (&player->deltatime, deltatime);

	player->start_msec = player->cur_msec;
	player->start_ticks = player->cur_ticks;


}

/**
 * Set the tempo of a MIDI player.
 * The player can be controlled by internal tempo coming from MIDI file tempo
 * change or controlled by external tempo expressed in BPM or in micro seconds
 * per quarter note.
 *
 * @param player MIDI player instance. Must be a valid pointer.
 * @param tempo_type Must a be value of #fluid_player_set_tempo_type and indicates the
 *  meaning of tempo value and how the player will be controlled, see below.
 * @param tempo Tempo value or multiplier.
 * 
 *  #FLUID_PLAYER_TEMPO_INTERNAL, the player will be controlled by internal
 *  MIDI file tempo changes. If there are no tempo change in the MIDI file a default
 *  value of 120 bpm is used. The @c tempo parameter is used as a multiplier factor
 *  that must be in the range (0.001 to 1000).
 *  For example, if the current MIDI file tempo is 120 bpm and the multiplier
 *  value is 0.5 then this tempo will be slowed down to 60 bpm.
 *  At creation, the player is set to be controlled by internal tempo with a
 *  multiplier factor set to 1.0.
 *
 *  #FLUID_PLAYER_TEMPO_EXTERNAL_BPM, the player will be controlled by the
 *  external tempo value provided  by the tempo parameter in bpm
 *  (i.e in quarter notes per minute) which must be in the range (1 to 60000000).
 *
 *  #FLUID_PLAYER_TEMPO_EXTERNAL_MIDI, similar as FLUID_PLAYER_TEMPO_EXTERNAL_BPM,
 *  but the tempo parameter value is in  micro seconds per quarter note which
 *  must be in the range (1 to 60000000).
 *  Using micro seconds per quarter note is convenient when the tempo value is
 *  derived from MIDI clock realtime messages.
 *
 * @note When the player is controlled by an external tempo
 * (#FLUID_PLAYER_TEMPO_EXTERNAL_BPM or #FLUID_PLAYER_TEMPO_EXTERNAL_MIDI) it
 * continues to memorize the most recent internal tempo change coming from the
 * MIDI file so that next call to fluid_player_set_tempo() with
 * #FLUID_PLAYER_TEMPO_INTERNAL will set the player to follow this internal
 * tempo.
 *
 * @return #FLUID_OK if success or #FLUID_FAILED otherwise (incorrect parameters).
 * @since 2.2.0
 */
int fluid_player_set_tempo (fluid_player_t * player, int tempo_type,
														double tempo) {
	fluid_return_val_if_fail (player != NULL, FLUID_FAILED);
	fluid_return_val_if_fail (tempo_type >= FLUID_PLAYER_TEMPO_INTERNAL,
														FLUID_FAILED);
	fluid_return_val_if_fail (tempo_type < FLUID_PLAYER_TEMPO_NBR,
														FLUID_FAILED);

	switch (tempo_type) {
		/* set the player to be driven by internal tempo coming from MIDI file */
	case FLUID_PLAYER_TEMPO_INTERNAL:
		/* check if the multiplier is in correct range */
		fluid_return_val_if_fail (tempo >= MIN_TEMPO_MULTIPLIER, FLUID_FAILED);
		fluid_return_val_if_fail (tempo <= MAX_TEMPO_MULTIPLIER, FLUID_FAILED);

		/* set the tempo multiplier */
		fluid_atomic_float_set (&player->multempo, (float) tempo);
		fluid_atomic_int_set (&player->sync_mode, 1);	/* set internal mode */
		break;

		/* set the player to be driven by external tempo */
	case FLUID_PLAYER_TEMPO_EXTERNAL_BPM:	/* value in bpm */
	case FLUID_PLAYER_TEMPO_EXTERNAL_MIDI:	/* value in us/quarter note */
		/* check if tempo is in correct range */
		fluid_return_val_if_fail (tempo >= MIN_TEMPO_VALUE, FLUID_FAILED);
		fluid_return_val_if_fail (tempo <= MAX_TEMPO_VALUE, FLUID_FAILED);

		/* set the tempo value */
		if (tempo_type == FLUID_PLAYER_TEMPO_EXTERNAL_BPM) {
			tempo = 60000000L / tempo;	/* convert tempo in us/quarter note */
		}
		fluid_atomic_int_set (&player->exttempo, (int) tempo);
		fluid_atomic_int_set (&player->sync_mode, 0);	/* set external mode */
		break;

	default:											/* shouldn't happen */
		break;
	}

	/* update deltatime depending of current tempo */
	fluid_player_update_tempo (player);

	return FLUID_OK;
}

/**
 * Set the tempo of a MIDI player.
 * @param player MIDI player instance
 * @param tempo Tempo to set playback speed to (in microseconds per quarter note, as per MIDI file spec)
 * @return Always returns #FLUID_OK
 * @note Tempo change events contained in the MIDI file can override the specified tempo at any time!
 * @deprecated Use fluid_player_set_tempo() instead.
 */
int fluid_player_set_midi_tempo (fluid_player_t * player, int tempo) {
	player->miditempo = tempo;

	fluid_player_update_tempo (player);
	return FLUID_OK;
}

/**
 * Set the tempo of a MIDI player in beats per minute.
 * @param player MIDI player instance
 * @param bpm Tempo in beats per minute
 * @return Always returns #FLUID_OK
 * @note Tempo change events contained in the MIDI file can override the specified BPM at any time!
 * @deprecated Use fluid_player_set_tempo() instead.
 */
int fluid_player_set_bpm (fluid_player_t * player, int bpm) {
	if (bpm <= 0) {
		return FLUID_FAILED;				/* to avoid a division by 0 */
	}

	return fluid_player_set_midi_tempo (player, 60000000L / bpm);
}

/**
 * Wait for a MIDI player until the playback has been stopped.
 * @param player MIDI player instance
 * @return Always #FLUID_OK
 *
 * @warning The player may still be used by a concurrently running synth context. Hence it is
 * not safe to immediately delete the player afterwards. Also refer to delete_fluid_player().
 */
int fluid_player_join (fluid_player_t * player) {
	while (fluid_player_get_status (player) != FLUID_PLAYER_DONE) {
		fluid_msleep (10);
	}
	return FLUID_OK;
}

/**
 * Get the number of tempo ticks passed.
 * @param player MIDI player instance
 * @return The number of tempo ticks passed
 * @since 1.1.7
 */
int fluid_player_get_current_tick (fluid_player_t * player) {
	return player->cur_ticks;
}

/**
 * Looks through all available MIDI tracks and gets the absolute tick of the very last event to play.
 * @param player MIDI player instance
 * @return Total tick count of the sequence
 * @since 1.1.7
 */
int fluid_player_get_total_ticks (fluid_player_t * player) {
	int i;
	int maxTicks = 0;

	for (i = 0; i < player->ntracks; i++) {
		if (player->track[i] != NULL) {
			int ticks = fluid_track_get_duration (player->track[i]);

			if (ticks > maxTicks) {
				maxTicks = ticks;
			}
		}
	}

	return maxTicks;
}

/**
 * Get the tempo currently used by a MIDI player.
 * The player can be controlled by internal tempo coming from MIDI file tempo
 * change or controlled by external tempo see fluid_player_set_tempo().
 * @param player MIDI player instance. Must be a valid pointer.
 * @return MIDI player tempo in BPM or FLUID_FAILED if error.
 * @since 1.1.7
 */
int fluid_player_get_bpm (fluid_player_t * player) {

	int midi_tempo = fluid_player_get_midi_tempo (player);

	if (midi_tempo > 0) {
		midi_tempo = 60000000L / midi_tempo;	/* convert in bpm */
	}

	return midi_tempo;
}

/**
 * Get the tempo currently used by a MIDI player.
 * The player can be controlled by internal tempo coming from MIDI file tempo
 * change or controlled by external tempo see fluid_player_set_tempo().

 * @param player MIDI player instance. Must be a valid pointer.
 * @return Tempo of the MIDI player (in microseconds per quarter note, as per
 * MIDI file spec) or FLUID_FAILED if error.
 * @since 1.1.7
 */
int fluid_player_get_midi_tempo (fluid_player_t * player) {
	int midi_tempo;								/* value to return */

	fluid_return_val_if_fail (player != NULL, FLUID_FAILED);

	midi_tempo = fluid_atomic_int_get (&player->exttempo);
	/* look if the player is internally synced */
	if (fluid_atomic_int_get (&player->sync_mode)) {
		midi_tempo = (int) ((float) fluid_atomic_int_get (&player->miditempo) /
												fluid_atomic_float_get (&player->multempo));
	}

	return midi_tempo;
}

/************************************************************************
 *       MIDI PARSER
 *
 */

/*
 * new_fluid_midi_parser
 */
fluid_midi_parser_t *new_fluid_midi_parser () {
	fluid_midi_parser_t *parser;
	parser = FLUID_NEW (fluid_midi_parser_t);

	if (parser == NULL) {
		return NULL;
	}

	parser->status = 0;						/* As long as the status is 0, the parser won't do anything -> no need to initialize all the fields. */
	return parser;
}

/*
 * delete_fluid_midi_parser
 */
void delete_fluid_midi_parser (fluid_midi_parser_t * parser) {
	fluid_return_if_fail (parser != NULL);

	FLUID_FREE (parser);
}
