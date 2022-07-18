#include "midi.h"
#include "sys.h"
#include "synth.h"

static int midiEventLength (unsigned char event);
static int isasciistring (char *s);
static long getlength (const unsigned char *s);


/* Read the entire contents of a file into memory, allocating enough memory
 * for the file, and returning the length and the buffer.
 * Note: This rewinds the file to the start before reading.
 * Returns NULL if there was an error reading or allocating memory.
 */
#define READ_FULL_INITIAL_BUFLEN 1024

static int trackReset (TrackT * track);

static int playerCallback (void *data, unsigned int msec);
static int playerReset (PlayerT * player);
static void playerUpdateTempo (PlayerT * player);

/*
 * trackSendEvents
 */
static void trackSendEvents (TrackT * track, Synthesizer * synth, PlayerT * player, unsigned int ticks, int seekTicks) {
	MidiEventT *event;
	int seeking = seekTicks >= 0;

	if (seeking) {
		ticks = seekTicks;					/* update target ticks */

		if (track->ticks > ticks) {
			trackReset (track);	/* reset track if seeking backwards */
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
			if (player->playbackCallback) {
				player->playbackCallback (player->playbackUserdata, event);
				if (event->type == NOTE_ON && event->param2 != 0
						&& !player->channelIsplaying[event->channel]) {
					player->channelIsplaying[event->channel] = TRUE;
				}
			}
		}

		if (event->type == MIDI_SET_TEMPO && player != NULL) {
			/* memorize the tempo change value coming from the MIDI file */
			atomic_int_set (&player->miditempo, event->param1);
			playerUpdateTempo (player);
		}

    track->cur = track->cur->next;

	}
}

/******************************************************
 *
 *     player
 */
static void
playerHandleResetSynth (void *data, const char *name, int value) {
	PlayerT *player = data;
	if (player != NULL)
    player->resetSynthBetweenSongs = value;
}

/**
 * Create a new MIDI player.
 * @param synth  synthesizer instance to create player for
 * @return New MIDI player instance or NULL on error (out of memory)
 */
PlayerT *newPlayer (Synthesizer * synth) {
	int i;
	PlayerT *player;
	player = NEW (PlayerT);

	if (player == NULL) {
		return NULL;
	}

	atomic_int_set (&player->status, PLAYER_READY);
	atomic_int_set (&player->stopping, 0);
	player->loop = 1;
	player->ntracks = 0;

	for (i = 0; i < MAX_NUMBER_OF_TRACKS; i++) {
		player->track[i] = NULL;
	}

	player->synth = synth;
	player->systemTimer = NULL;
	player->sampleTimer = NULL;
	player->division = 0;

	/* internal tempo (from MIDI file) in micro seconds per quarter note */
	player->syncMode = 1;				/* the player follows internal tempo change */
	player->miditempo = 500000;
	/* external tempo in micro seconds per quarter note */
	player->exttempo = 500000;
	/* tempo multiplier */
	player->multempo = 1.0F;

	player->deltatime = 4.0;
	player->curMsec = 0;
	player->curTicks = 0;
	player->lastCallbackTicks = -1;
	atomic_int_set (&player->seekTicks, -1);
	playerSetPlaybackCallback (player, synthHandleMidiEvent,
																			synth);
	playerSetTickCallback (player, NULL, NULL);
	player->useSystemTimer = settingsStrEqual (synth->settings,
																											 "player.timing-source",
																											 "system");
	if (player->useSystemTimer) {
		player->systemTimer = newTimer ((int) player->deltatime,
																						playerCallback, player,
																						TRUE, FALSE, TRUE);

		if (player->systemTimer == NULL) {
			goto err;
		}
	} else {
		player->sampleTimer = new_Sampleimer (player->synth,
																									 playerCallback,
																									 player);

		if (player->sampleTimer == NULL) {
			goto err;
		}
	}

	settingsGetint (synth->settings, "player.reset-synth", &i);
	playerHandleResetSynth (player, NULL, i);

	settingsCallbackInt (synth->settings, "player.reset-synth",
															 playerHandleResetSynth, player);

	return player;

err:
	deletePlayer (player);
	return NULL;
}

/**
 * Delete a MIDI player instance.
 * @param player MIDI player instance
 * @warning Do not call when the synthesizer associated to this \p player renders audio,
 * i.e. an audio driver is running or any other synthesizer thread concurrently calls
 * synthProcess() or synthNwriteFloat() or synthWrite_*() !
 */
void deletePlayer (PlayerT * player) {
	listT *q;
	playlistItem *pi;

	returnIfFail (player == NULL)
    return;

	settingsCallbackInt (player->synth->settings, "player.reset-synth",
															 NULL, NULL);

	playerStop (player);
	playerReset (player);

	deleteTimer (player->systemTimer);
	delete_Sampleimer (player->synth, player->sampleTimer);

	while (player->playlist != NULL) {
		q = player->playlist->next;
		pi = (playlistItem *) player->playlist->data;
		FREE (pi->filename);
		FREE (pi->buffer);
		FREE (pi);
		delete1_List (player->playlist);
		player->playlist = q;
	}

	FREE (player);
}

/**
 * Registers settings related to the MIDI player
 */
void playerSettings (Settings * settings) {
	/* player.timing-source can be either "system" (use system timer)
	   or "sample" (use timer based on number of written samples) */
	settingsRegisterStr (settings, "player.timing-source", "sample", 0);
	settingsAddOption (settings, "player.timing-source", "sample");
	settingsAddOption (settings, "player.timing-source", "system");

	/* Selects whether the player should reset the synth between songs, or not. */
	settingsRegisterInt (settings, "player.reset-synth", 1, 0, 1,
															 HINT_TOGGLED);
}


int playerReset (PlayerT * player) {
	int i;

	for (i = 0; i < MAX_NUMBER_OF_TRACKS; i++) {
		if (player->track[i] != NULL) {
			deleteTrack (player->track[i]);
			player->track[i] = NULL;
		}
	}

	for (i = 0; i < MAX_NUMBER_OF_CHANNELS; i++) {
		player->channelIsplaying[i] = FALSE;
	}

	/*    player->currentFile = NULL; */
	/*    player->status = PLAYER_READY; */
	/*    player->loop = 1; */
	player->ntracks = 0;
	player->division = 0;
	player->miditempo = 500000;
	player->deltatime = 4.0;
	return 0;
}

/*
 * playerAddTrack
 */
int playerAddTrack (PlayerT * player, TrackT * track) {
	if (player->ntracks < MAX_NUMBER_OF_TRACKS) {
		player->track[player->ntracks++] = track;
		return OK;
	} else {
		return FAILED;
	}
}

/**
 * Change the MIDI callback function.
 *
 * @param player MIDI player instance
 * @param handler Pointer to callback function
 * @param handlerData Parameter sent to the callback function
 * @returns OK
 *
 * This is usually set to synthHandleMidiEvent(), but can optionally
 * be changed to a user-defined function instead, for intercepting all MIDI
 * messages sent to the synth. You can also use a midi router as the callback
 * function to modify the MIDI messages before sending them to the synth.
 *
 * @since 1.1.4
 */
int
playerSetPlaybackCallback (PlayerT * player,
																		handleMidiEventFuncT handler,
																		void *handlerData) {
	player->playbackCallback = handler;
	player->playbackUserdata = handlerData;
	return OK;
}

/**
 * Add a listener function for every MIDI tick change.
 *
 * @param player MIDI player instance
 * @param handler Pointer to callback function
 * @param handlerData Opaque parameter to be sent to the callback function
 * @returns #OK
 *
 * This callback is not set by default, but can optionally
 * be changed to a user-defined function for intercepting all MIDI
 * tick changes and react to them with precision.
 *
 * @since 2.2.0
 */
int
playerSetTickCallback (PlayerT * player,
																handleMidiTickFuncT handler,
																void *handlerData) {
	player->tickCallback = handler;
	player->tickUserdata = handlerData;
	return OK;
}

/**
 * Add a MIDI file to a player queue.
 * @param player MIDI player instance
 * @param midifile File name of the MIDI file to add
 * @return #OK or #FAILED
 */
int playerAdd (PlayerT * player, const char *midifile) {
	playlistItem *pi = MALLOC (sizeof (playlistItem));
	char *f = STRDUP (midifile);

	if (!pi || !f) {
		FREE (pi);
		FREE (f);
		return FAILED;
	}

	pi->filename = f;
	pi->buffer = NULL;
	pi->bufferLen = 0;
	player->playlist = listAppend (player->playlist, pi);
	return OK;
}

/**
 * Add a MIDI file to a player queue, from a buffer in memory.
 * @param player MIDI player instance
 * @param buffer Pointer to memory containing the bytes of a complete MIDI
 *   file. The data is copied, so the caller may free or modify it immediately
 *   without affecting the playlist.
 * @param len Length of the buffer, in bytes.
 * @return #OK or #FAILED
 */
int
playerAddMem (PlayerT * player, const void *buffer,
											size_t len) {
	/* Take a copy of the buffer, so the caller can free immediately. */
	playlistItem *pi = MALLOC (sizeof (playlistItem));
	void *bufCopy = MALLOC (len);

	if (!pi || !bufCopy) {
		FREE (pi);
		FREE (bufCopy);
		return FAILED;
	}

	MEMCPY (bufCopy, buffer, len);
	pi->filename = NULL;
	pi->buffer = bufCopy;
	pi->bufferLen = len;
	player->playlist = listAppend (player->playlist, pi);
	return OK;
}

/*
 * playerLoad
 */
int playerLoad (PlayerT * player, playlistItem * item) {
	midiFile *midifile;
	char *buffer;
	size_t bufferLength;
	int bufferOwned;

	if (item->filename != NULL) {
		file fp;
		/* This file is specified by filename; load the file from disk */
		/* Read the entire contents of the file into the buffer */
		fp = FOPEN (item->filename, "rb");

		if (fp == NULL) {
			return FAILED;
		}

		buffer = fileReadFull (fp, &bufferLength);

		FCLOSE (fp);

		if (buffer == NULL) {
			return FAILED;
		}

		bufferOwned = 1;
	} else {
		/* This file is specified by a pre-loaded buffer; load from memory */
		buffer = (char *) item->buffer;
		bufferLength = item->bufferLen;
		/* Do not free the buffer (it is owned by the playlist) */
		bufferOwned = 0;
	}

	midifile = newMidiFile (buffer, bufferLength);

	if (midifile == NULL) {
		if (bufferOwned) {
			FREE (buffer);
		}

		return FAILED;
	}

	player->division = midiFileGetDivision (midifile);
	playerUpdateTempo (player);	// Update deltatime

	if (midiFileLoadTracks (midifile, player) != OK) {
		if (bufferOwned) {
			FREE (buffer);
		}

		deleteMidiFile (midifile);
		return FAILED;
	}

	deleteMidiFile (midifile);

	if (bufferOwned) {
		FREE (buffer);
	}

	return OK;
}

void playerAdvancefile (PlayerT * player) {
	if (player->playlist == NULL) {
		return;											/* No files to play */
	}

	if (player->currentfile != NULL) {
		player->currentfile = listNext (player->currentfile);
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

void playerPlaylistLoad (PlayerT * player, unsigned int msec) {
	playlistItem *currentPlayitem;
	int i;

	do {
		playerAdvancefile (player);

		if (player->currentfile == NULL) {
			/* Failed to find next song, probably since we're finished */
			atomic_int_set (&player->status, PLAYER_DONE);
			return;
		}

		playerReset (player);
		currentPlayitem = (playlistItem *) player->currentfile->data;
	}
	while (playerLoad (player, currentPlayitem) != OK);

	/* Successfully loaded midi file */

	player->beginMsec = msec;
	player->startMsec = msec;
	player->startTicks = 0;
	player->curTicks = 0;

	for (i = 0; i < player->ntracks; i++) {
		if (player->track[i] != NULL) {
			trackReset (player->track[i]);
		}
	}
}

/*
 * playerCallback
 */
int playerCallback (void *data, unsigned int msec) {
	int i;
	int loadnextfile;
	int status = PLAYER_DONE;
	midiEventT muteEvent;
	PlayerT *player;
	Synthesizer *synth;
	player = (PlayerT *) data;
	synth = player->synth;

	loadnextfile = player->currentfile == NULL ? 1 : 0;

	midiEventSetType (&muteEvent, CONTROL_CHANGE);
	muteEvent.param1 = ALL_SOUND_OFF;
	muteEvent.param2 = 1;

	if (playerGetStatus (player) != PLAYER_PLAYING) {
		if (atomicIntGet (&player->stopping)) {
			for (i = 0; i < synth->midiChannels; i++) {
				if (player->channelIsplaying[i]) {
					midiEventSetChannel (&muteEvent, i);
					player->playbackCallback (player->playbackUserdata, &muteEvent);
				}
			}
			atomic_int_set (&player->stopping, 0);
		}
		return 1;
	}
	do {
		float deltatime;
		int seekTicks;

		if (loadnextfile) {
			loadnextfile = 0;
			playerPlaylistLoad (player, msec);

			if (player->currentfile == NULL) {
				return 0;
			}
		}

		player->curMsec = msec;
		deltatime = atomicFloatGet (&player->deltatime);
		player->curTicks = (player->startTicks
												 +
												 (int) ((double)
																(player->curMsec - player->startMsec)
																/ deltatime + 0.5));	/* 0.5 to average overall error when casting */

		seekTicks = atomicIntGet (&player->seekTicks);
		if (seekTicks >= 0) {
			for (i = 0; i < synth->midiChannels; i++) {
				if (player->channelIsplaying[i]) {
					midiEventSetChannel (&muteEvent, i);
					player->playbackCallback (player->playbackUserdata, &muteEvent);
				}
			}
		}

		for (i = 0; i < player->ntracks; i++) {
			if (!trackEot (player->track[i])) {
				status = PLAYER_PLAYING;
				trackSendEvents (player->track[i], synth, player,
																 player->curTicks, seekTicks);
			}
		}

		if (seekTicks >= 0) {
			player->startTicks = seekTicks;	/* tick position of last tempo value (which is now) */
			player->curTicks = seekTicks;
			player->beginMsec = msec;	/* only used to calculate the duration of playing */
			player->startMsec = msec;	/* should be the (synth)-time of the last tempo change */
			atomic_int_set (&player->seekTicks, -1);	/* clear seekTicks */
		}

		if (status == PLAYER_DONE) {

			if (player->resetSynthBetweenSongs) {
				synthSystemReset (player->synth);
			}

			loadnextfile = 1;
		}

		if (player->tickCallback != NULL
				&& player->lastCallbackTicks != player->curTicks) {
			player->tickCallback (player->tickUserdata, player->curTicks);
			player->lastCallbackTicks = player->curTicks;
		}
	}
	while (loadnextfile);

	/* do not update the status if the player has been stopped already */
	atomicIntCompareAndExchange (&player->status,
																				 PLAYER_PLAYING, status);

	return 1;
}

/**
 * Activates play mode for a MIDI player if not already playing.
 * @param player MIDI player instance
 * @return #OK on success, #FAILED otherwise
 *
 * If the list of files added to the player has completed its requested number of loops,
 * the playlist will be restarted from the beginning with a loop count of 1.
 */
int playerPlay (PlayerT * player) {
	if (playerGetStatus (player) == PLAYER_PLAYING ||
			player->playlist == NULL) {
		return OK;
	}

	if (!player->useSystemTimer) {
		SampleimerReset (player->synth, player->sampleTimer);
	}

	/* If we're at the end of the playlist and there are no loops left, loop once */
	if (player->currentfile == NULL && player->loop == 0) {
		player->loop = 1;
	}

	atomic_int_set (&player->status, PLAYER_PLAYING);

	return OK;
}

/**
 * Pauses the MIDI playback.
 *
 * @param player MIDI player instance
 * @return Always returns #OK
 *
 * It will not rewind to the beginning of the file, use playerSeek() for this purpose.
 */
int playerStop (PlayerT * player) {
	atomic_int_set (&player->status, PLAYER_DONE);
	atomic_int_set (&player->stopping, 1);
	playerSeek (player, playerGetCurrentTick (player));
	return OK;
}

/**
 * Get MIDI player status.
 * @param player MIDI player instance
 * @return Player status (#playerStatus)
 * @since 1.1.0
 */
int playerGetStatus (PlayerT * player) {
	return atomicIntGet (&player->status);
}

/**
 * Seek in the currently playing file.
 *
 * @param player MIDI player instance
 * @param ticks the position to seek to in the current file
 * @return #FAILED if ticks is negative or after the latest tick of the file
 * [or, since 2.1.3, if another seek operation is currently in progress],
 * #OK otherwise.
 *
 * The actual seek will be performed when the synth calls back the player (i.e. a few
 * levels above the player's callback set with playerSetPlaybackCallback()).
 * If the player's status is #PLAYER_PLAYING and a previous seek operation has
 * not been completed yet, #FAILED is returned.
 *
 * @since 2.0.0
 */
int playerSeek (PlayerT * player, int ticks) {
	if (ticks < 0
			|| (playerGetStatus (player) != PLAYER_READY
					&& ticks > playerGetTotalTicks (player))) {
		return FAILED;
	}

	if (playerGetStatus (player) == PLAYER_PLAYING) {
		if (atomicIntCompareAndExchange
				(&player->seekTicks, -1, ticks)) {
			// new seek position has been set, as no previous seek was in progress
			return OK;
		}
	} else {
		// If the player is not currently playing, a new seek position can be set at any time. This allows
		// the user to do:
		// playerStop();
		// playerSeek(0); // to beginning
		atomic_int_set (&player->seekTicks, ticks);
		return OK;
	}

	// a previous seek is still in progress or hasn't been processed yet
	return FAILED;
}


/**
 * Enable looping of a MIDI player
 *
 * @param player MIDI player instance
 * @param loop Times left to loop the playlist. -1 means loop infinitely.
 * @return Always returns #OK
 *
 * For example, if you want to loop the playlist twice, set loop to 2
 * and call this function before you start the player.
 *
 * @since 1.1.0
 */
int playerSetLoop (PlayerT * player, int loop) {
	player->loop = loop;
	return OK;
}

/**
 * update the MIDI player internal deltatime dependent of actual tempo.
 * @param player MIDI player instance
 */
static void playerUpdateTempo (PlayerT * player) {
	int tempo;										/* tempo in micro seconds by quarter note */
	float deltatime;

	if (atomicIntGet (&player->syncMode)) {
		/* take internal tempo from MIDI file */
		tempo = atomicIntGet (&player->miditempo);
		/* compute deltattime (in ms) from current tempo and apply tempo multiplier */
		deltatime = (float) tempo / (float) player->division / (float) 1000.0;
		deltatime /= atomicFloatGet (&player->multempo);	/* multiply tempo */
	} else {
		/* take  external tempo */
		tempo = atomicIntGet (&player->exttempo);
		/* compute deltattime (in ms) from current tempo */
		deltatime = (float) tempo / (float) player->division / (float) 1000.0;
	}

	atomicFloatSet (&player->deltatime, deltatime);

	player->startMsec = player->curMsec;
	player->startTicks = player->curTicks;


}

/**
 * Set the tempo of a MIDI player.
 * The player can be controlled by internal tempo coming from MIDI file tempo
 * change or controlled by external tempo expressed in BPM or in micro seconds
 * per quarter note.
 *
 * @param player MIDI player instance. Must be a valid pointer.
 * @param tempoType Must a be value of #playerSetTempoType and indicates the
 *  meaning of tempo value and how the player will be controlled, see below.
 * @param tempo Tempo value or multiplier.
 * 
 *  #PLAYER_TEMPO_INTERNAL, the player will be controlled by internal
 *  MIDI file tempo changes. If there are no tempo change in the MIDI file a default
 *  value of 120 bpm is used. The @c tempo parameter is used as a multiplier factor
 *  that must be in the range (0.001 to 1000).
 *  For example, if the current MIDI file tempo is 120 bpm and the multiplier
 *  value is 0.5 then this tempo will be slowed down to 60 bpm.
 *  At creation, the player is set to be controlled by internal tempo with a
 *  multiplier factor set to 1.0.
 *
 *  #PLAYER_TEMPO_EXTERNAL_BPM, the player will be controlled by the
 *  external tempo value provided  by the tempo parameter in bpm
 *  (i.e in quarter notes per minute) which must be in the range (1 to 60000000).
 *
 *  #PLAYER_TEMPO_EXTERNAL_MIDI, similar as PLAYER_TEMPO_EXTERNAL_BPM,
 *  but the tempo parameter value is in  micro seconds per quarter note which
 *  must be in the range (1 to 60000000).
 *  Using micro seconds per quarter note is convenient when the tempo value is
 *  derived from MIDI clock realtime messages.
 *
 * @note When the player is controlled by an external tempo
 * (#PLAYER_TEMPO_EXTERNAL_BPM or #PLAYER_TEMPO_EXTERNAL_MIDI) it
 * continues to memorize the most recent internal tempo change coming from the
 * MIDI file so that next call to playerSetTempo() with
 * #PLAYER_TEMPO_INTERNAL will set the player to follow this internal
 * tempo.
 *
 * @return #OK if success or #FAILED otherwise (incorrect parameters).
 * @since 2.2.0
 */
int playerSetTempo (PlayerT * player, int tempoType,
														double tempo) {
	returnValIfFail (player != NULL, FAILED);
	returnValIfFail (tempoType >= PLAYER_TEMPO_INTERNAL,
														FAILED);
	returnValIfFail (tempoType < PLAYER_TEMPO_NBR,
														FAILED);

	switch (tempoType) {
		/* set the player to be driven by internal tempo coming from MIDI file */
	case PLAYER_TEMPO_INTERNAL:
		/* check if the multiplier is in correct range */
		returnValIfFail (tempo >= MIN_TEMPO_MULTIPLIER, FAILED);
		returnValIfFail (tempo <= MAX_TEMPO_MULTIPLIER, FAILED);

		/* set the tempo multiplier */
		atomicFloatSet (&player->multempo, (float) tempo);
		atomic_int_set (&player->syncMode, 1);	/* set internal mode */
		break;

		/* set the player to be driven by external tempo */
	case PLAYER_TEMPO_EXTERNAL_BPM:	/* value in bpm */
	case PLAYER_TEMPO_EXTERNAL_MIDI:	/* value in us/quarter note */
		/* check if tempo is in correct range */
		returnValIfFail (tempo >= MIN_TEMPO_VALUE, FAILED);
		returnValIfFail (tempo <= MAX_TEMPO_VALUE, FAILED);

		/* set the tempo value */
		if (tempoType == PLAYER_TEMPO_EXTERNAL_BPM) {
			tempo = 60000000L / tempo;	/* convert tempo in us/quarter note */
		}
		atomic_int_set (&player->exttempo, (int) tempo);
		atomic_int_set (&player->syncMode, 0);	/* set external mode */
		break;

	default:											/* shouldn't happen */
		break;
	}

	/* update deltatime depending of current tempo */
	playerUpdateTempo (player);

	return OK;
}

/**
 * Set the tempo of a MIDI player.
 * @param player MIDI player instance
 * @param tempo Tempo to set playback speed to (in microseconds per quarter note, as per MIDI file spec)
 * @return Always returns #OK
 * @note Tempo change events contained in the MIDI file can override the specified tempo at any time!
 * @deprecated Use playerSetTempo() instead.
 */
int playerSetMidiTempo (PlayerT * player, int tempo) {
	player->miditempo = tempo;

	playerUpdateTempo (player);
	return OK;
}

/**
 * Set the tempo of a MIDI player in beats per minute.
 * @param player MIDI player instance
 * @param bpm Tempo in beats per minute
 * @return Always returns #OK
 * @note Tempo change events contained in the MIDI file can override the specified BPM at any time!
 * @deprecated Use playerSetTempo() instead.
 */
int playerSetBpm (PlayerT * player, int bpm) {
	if (bpm <= 0) {
		return FAILED;				/* to avoid a division by 0 */
	}

	return playerSetMidiTempo (player, 60000000L / bpm);
}

/**
 * Wait for a MIDI player until the playback has been stopped.
 * @param player MIDI player instance
 * @return Always #OK
 *
 * @warning The player may still be used by a concurrently running synth context. Hence it is
 * not safe to immediately delete the player afterwards. Also refer to deletePlayer().
 */
int playerJoin (PlayerT * player) {
	while (playerGetStatus (player) != PLAYER_DONE) {
		msleep (10);
	}
	return OK;
}

/**
 * Get the number of tempo ticks passed.
 * @param player MIDI player instance
 * @return The number of tempo ticks passed
 * @since 1.1.7
 */
int playerGetCurrentTick (PlayerT * player) {
	return player->curTicks;
}

/**
 * Looks through all available MIDI tracks and gets the absolute tick of the very last event to play.
 * @param player MIDI player instance
 * @return Total tick count of the sequence
 * @since 1.1.7
 */
int playerGetTotalTicks (PlayerT * player) {
	int i;
	int maxTicks = 0;

	for (i = 0; i < player->ntracks; i++) {
		if (player->track[i] != NULL) {
			int ticks = trackGetDuration (player->track[i]);

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
 * change or controlled by external tempo see playerSetTempo().
 * @param player MIDI player instance. Must be a valid pointer.
 * @return MIDI player tempo in BPM or FAILED if error.
 * @since 1.1.7
 */
int playerGetBpm (PlayerT * player) {

	int midiTempo = playerGetMidiTempo (player);

	if (midiTempo > 0) {
		midiTempo = 60000000L / midiTempo;	/* convert in bpm */
	}

	return midiTempo;
}

/**
 * Get the tempo currently used by a MIDI player.
 * The player can be controlled by internal tempo coming from MIDI file tempo
 * change or controlled by external tempo see playerSetTempo().

 * @param player MIDI player instance. Must be a valid pointer.
 * @return Tempo of the MIDI player (in microseconds per quarter note, as per
 * MIDI file spec) or FAILED if error.
 * @since 1.1.7
 */
int playerGetMidiTempo (PlayerT * player) {
	int midiTempo;								/* value to return */

	returnValIfFail (player != NULL, FAILED);

	midiTempo = atomicIntGet (&player->exttempo);
	/* look if the player is internally synced */
	if (atomicIntGet (&player->syncMode)) {
		midiTempo = (int) ((float) atomicIntGet (&player->miditempo) /
												atomicFloatGet (&player->multempo));
	}

	return midiTempo;
}

/************************************************************************
 *       MIDI PARSER
 *
 */

/*
 * newMidiParser
 */
MidiParserT *newMidiParser () {
	MidiParserT *parser;
	parser = NEW (MidiParserT);

	if (parser == NULL) {
		return NULL;
	}

	parser->status = 0;						/* As long as the status is 0, the parser won't do anything -> no need to initialize all the fields. */
	return parser;
}

/*
 * deleteMidiParser
 */
void deleteMidiParser (MidiParserT * parser) {
	if (parser != NULL)
    FREE (parser);
}
