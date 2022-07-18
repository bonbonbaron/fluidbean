#ifndef _MIDI_H
#define _MIDI_H

#include "fluidbean.h"
#include "synth.h"

struct _fluidMidiEventT;
typedef int (*handleMidiEventFuncT)(void *data, struct _fluidMidiEventT *event);
typedef int (*handleMidiTickFuncT)(void *data, int tick);
typedef enum {
    MIDI_ROUTER_RULE_NOTE,                  /**< MIDI note rule */
    MIDI_ROUTER_RULE_CC,                    /**< MIDI controller rule */
    MIDI_ROUTER_RULE_PROG_CHANGE,           /**< MIDI program change rule */
    MIDI_ROUTER_RULE_PITCH_BEND,            /**< MIDI pitch bend rule */
    MIDI_ROUTER_RULE_CHANNEL_PRESSURE,      /**< MIDI channel pressure rule */
    MIDI_ROUTER_RULE_KEY_PRESSURE,          /**< MIDI key pressure rule */
    MIDI_ROUTER_RULE_COUNT                  /**< @internal Total count of rule types. This symbol
                                                    is not part of the public API and ABI stability
                                                    guarantee and may change at any time!*/
} MidiRouterRuleType;

enum playerStatus {
    PLAYER_READY,           /**< Player is ready */
    PLAYER_PLAYING,         /**< Player is currently playing */
    PLAYER_STOPPING,        /**< Player is stopping, but hasn't finished yet (currently unused) */
    PLAYER_DONE             /**< Player is finished playing */
};

enum playerSetTempoType {
    PLAYER_TEMPO_INTERNAL,      /**< Use Midi file tempo set in Midi file (120 bpm by default). Multiplied by a factor */
    PLAYER_TEMPO_EXTERNAL_BPM,  /**< Set player tempo in bpm, supersede Midi file tempo */
    PLAYER_TEMPO_EXTERNAL_MIDI, /**< Set player tempo in us per quarter note, supersede Midi file tempo */
    PLAYER_TEMPO_NBR        /**< @internal Value defines the count of player tempo type (#playerSetTempoType) @warning This symbol is not part of the public API and ABI stability guarantee and may change at any time! */
};



#define MAX_NUMBER_OF_TRACKS 128
#define MAX_NUMBER_OF_CHANNELS 16



/*
 * MidiEventT
 */
typedef struct _MidiEventT {
    struct _MidiEventT *next; /* Link to next event */
    void *paramptr;           /* Pointer parameter (for SYSEX data), size is stored to param1, param2 indicates if pointer should be freed (dynamic if TRUE) */
    unsigned int dtime;       /* Delay (ticks) between this and previous event. midi tracks. */
    unsigned int param1;      /* First parameter */
    unsigned int param2;      /* Second parameter */
    unsigned char type;       /* MIDI event type */
    unsigned char channel;    /* MIDI channel */
} MidiEventT;

typedef struct _fluidTrackT {
    char *name;
    int num;
    MidiEventT *first;
    MidiEventT *cur;
    MidiEventT *last;
    unsigned int ticks;
} TrackT;

struct _MidiParserT *newMidiParser (void);
void deleteMidiParser (struct _MidiParserT * parser);
MidiEventT *MidiParserParse (struct _MidiParserT * parser,
																						 U8 c);

typedef struct _fluidSampleTimerT {
	struct _fluidSampleTimerT *next;		/* Single linked list of timers */
	unsigned long starttick;
	//timerCallbackT callback;
	void *data;
	int isfinished;
} sampleTimerT;


/* * player */
typedef struct _PlayerT {
    int status;
    int stopping; /* Flag for sending allNotesOff when player is stopped */
    int ntracks;
    TrackT *track[MAX_NUMBER_OF_TRACKS];
    struct _Synthesizer *synth;
    struct _fluidTimerT *systemTimer;
    sampleTimerT *sampleTimer;

    int loop; /* -1 = loop infinitely, otherwise times left to loop the playlist */

    char useSystemTimer;   /* if zero, use sample timers, otherwise use system clock timer */
    char resetSynthBetweenSongs; /* 1 if system reset should be sent to the synth between songs. */
    int seekTicks; /* new position in tempo ticks (Midi ticks) for seeking */
    int startTicks;          /* the number of tempo ticks passed at the last tempo change */
    int curTicks;            /* the number of tempo ticks passed */
    int lastCallbackTicks;  /* the last tick number that was passed to player->tickCallback */
    int beginMsec;           /* the time (msec) of the beginning of the file */
    int startMsec;           /* the start time of the last tempo change */
    int curMsec;             /* the current time */
    /* sync mode: indicates the tempo mode the player is driven by (see playerSetTempo()):
       1, the player is driven by internal tempo (Miditempo). This is the default.
       0, the player is driven by external tempo (exttempo)
    */
    int syncMode;
    /* Miditempo: internal tempo coming from MIDI file tempo change events
      (in micro seconds per quarter note)
    */
    int miditempo;     /* as indicated by MIDI SetTempo: n 24th of a usec per Midi-clock. bravo! */
    /* exttempo: external tempo set by playerSetTempo() (in micro seconds per quarter note) */
    int exttempo;
    /* multempo: tempo multiplier set by playerSetTempo() */
    float multempo;
    float deltatime;   /* milliseconds per Midi tick. depends on current tempo mode (see syncMode) */
    unsigned int division;

    handleMidiEventFuncT playbackCallback; /* function fired on each Midi event as it is played */
    void *playbackUserdata; /* pointer to user-defined data passed to playbackCallback function */
    handleMidiTickFuncT tickCallback; /* function fired on each tick change */
    void *tickUserdata; /* pointer to user-defined data passed to tickCallback function */

    int channelIsplaying[MAX_NUMBER_OF_CHANNELS]; /* flags indicating channels on which notes have played */
} PlayerT;

S32 MidiSendEvent (struct _Synthesizer * synth, PlayerT * player,
													 MidiEventT * evt);


/***************************************************************
 *
 *                   CONSTANTS & ENUM
 */


#define MAX_NUMBER_OF_TRACKS 128

enum MidiEventType {
	/* channel messages */
	NOTE_OFF = 0x80,
	NOTE_ON = 0x90,
	KEY_PRESSURE = 0xa0,
	CONTROL_CHANGE = 0xb0,
	PROGRAM_CHANGE = 0xc0,
	CHANNEL_PRESSURE = 0xd0,
	PITCH_BEND = 0xe0,
	/* system exclusive */
	MIDI_SYSEX = 0xf0,
	/* system common - never in Midi files */
	MIDI_TIME_CODE = 0xf1,
	MIDI_SONG_POSITION = 0xf2,
	MIDI_SONG_SELECT = 0xf3,
	MIDI_TUNE_REQUEST = 0xf6,
	MIDI_EOX = 0xf7,
	/* system real-time - never in Midi files */
	MIDI_SYNC = 0xf8,
	MIDI_TICK = 0xf9,
	MIDI_START = 0xfa,
	MIDI_CONTINUE = 0xfb,
	MIDI_STOP = 0xfc,
	MIDI_ACTIVE_SENSING = 0xfe,
	MIDI_SYSTEM_RESET = 0xff,
	/* meta event - for Midi files only */
	MIDI_META_EVENT = 0xff
};

enum MidiControlChange {
	BANK_SELECT_MSB = 0x00,
	MODULATION_MSB = 0x01,
	BREATH_MSB = 0x02,
	FOOT_MSB = 0x04,
	PORTAMENTO_TIME_MSB = 0x05,
	DATA_ENTRY_MSB = 0x06,
	VOLUME_MSB = 0x07,
	BALANCE_MSB = 0x08,
	PAN_MSB = 0x0A,
	EXPRESSION_MSB = 0x0B,
	EFFECTS1_MSB = 0x0C,
	EFFECTS2_MSB = 0x0D,
	GPC1_MSB = 0x10,							/* general purpose controller */
	GPC2_MSB = 0x11,
	GPC3_MSB = 0x12,
	GPC4_MSB = 0x13,
	BANK_SELECT_LSB = 0x20,
	MODULATION_WHEEL_LSB = 0x21,
	BREATH_LSB = 0x22,
	FOOT_LSB = 0x24,
	PORTAMENTO_TIME_LSB = 0x25,
	DATA_ENTRY_LSB = 0x26,
	VOLUME_LSB = 0x27,
	BALANCE_LSB = 0x28,
	PAN_LSB = 0x2A,
	EXPRESSION_LSB = 0x2B,
	EFFECTS1_LSB = 0x2C,
	EFFECTS2_LSB = 0x2D,
	GPC1_LSB = 0x30,
	GPC2_LSB = 0x31,
	GPC3_LSB = 0x32,
	GPC4_LSB = 0x33,
	SUSTAIN_SWITCH = 0x40,
	PORTAMENTO_SWITCH = 0x41,
	SOSTENUTO_SWITCH = 0x42,
	SOFT_PEDAL_SWITCH = 0x43,
	LEGATO_SWITCH = 0x45,
	HOLD2_SWITCH = 0x45,
	SOUND_CTRL1 = 0x46,
	SOUND_CTRL2 = 0x47,
	SOUND_CTRL3 = 0x48,
	SOUND_CTRL4 = 0x49,
	SOUND_CTRL5 = 0x4A,
	SOUND_CTRL6 = 0x4B,
	SOUND_CTRL7 = 0x4C,
	SOUND_CTRL8 = 0x4D,
	SOUND_CTRL9 = 0x4E,
	SOUND_CTRL10 = 0x4F,
	GPC5 = 0x50,
	GPC6 = 0x51,
	GPC7 = 0x52,
	GPC8 = 0x53,
	PORTAMENTO_CTRL = 0x54,
	EFFECTS_DEPTH1 = 0x5B,
	EFFECTS_DEPTH2 = 0x5C,
	EFFECTS_DEPTH3 = 0x5D,
	EFFECTS_DEPTH4 = 0x5E,
	EFFECTS_DEPTH5 = 0x5F,
	DATA_ENTRY_INCR = 0x60,
	DATA_ENTRY_DECR = 0x61,
	NRPN_LSB = 0x62,
	NRPN_MSB = 0x63,
	RPN_LSB = 0x64,
	RPN_MSB = 0x65,
	ALL_SOUND_OFF = 0x78,
	ALL_CTRL_OFF = 0x79,
	LOCAL_CONTROL = 0x7A,
	ALL_NOTES_OFF = 0x7B,
	OMNI_OFF = 0x7C,
	OMNI_ON = 0x7D,
	POLY_OFF = 0x7E,
	POLY_ON = 0x7F
};

/* General MIDI RPN event numbers (LSB, MSB = 0) */
enum MidiRpnEvent {
	RPN_PITCH_BEND_RANGE = 0x00,
	RPN_CHANNEL_FINE_TUNE = 0x01,
	RPN_CHANNEL_COARSE_TUNE = 0x02,
	RPN_TUNING_PROGRAM_CHANGE = 0x03,
	RPN_TUNING_BANK_SELECT = 0x04,
	RPN_MODULATION_DEPTH_RANGE = 0x05
};

enum MidiMetaEvent {
	MIDI_COPYRIGHT = 0x02,
	MIDI_TRACK_NAME = 0x03,
	MIDI_INST_NAME = 0x04,
	MIDI_LYRIC = 0x05,
	MIDI_MARKER = 0x06,
	MIDI_CUE_POINT = 0x07,
	MIDI_EOT = 0x2f,
	MIDI_SET_TEMPO = 0x51,
	MIDI_SMPTE_OFFSET = 0x54,
	MIDI_TIME_SIGNATURE = 0x58,
	MIDI_KEY_SIGNATURE = 0x59,
	MIDI_SEQUENCER_EVENT = 0x7f
};

/* MIDI SYSEX useful manufacturer values */
enum MidiSysexManuf {
	MIDI_SYSEX_MANUF_ROLAND = 0x41,								/**< Roland manufacturer ID */
	MIDI_SYSEX_UNIV_NON_REALTIME = 0x7E,					/**< Universal non realtime message */
	MIDI_SYSEX_UNIV_REALTIME = 0x7F								/**< Universal realtime message */
};

#define MIDI_SYSEX_DEVICE_ID_ALL        0x7F		/**< Device ID used in SYSEX messages to indicate all devices */

/* SYSEX sub-ID #1 which follows device ID */
#define MIDI_SYSEX_MIDI_TUNING_ID       0x08		/**< Sysex sub-ID #1 for MIDI tuning messages */
#define MIDI_SYSEX_GM_ID                0x09		/**< Sysex sub-ID #1 for General MIDI messages */

/**
 * SYSEX tuning message IDs.
 */
enum MidiSysexTuningMsgId {
	MIDI_SYSEX_TUNING_BULK_DUMP_REQ = 0x00,				/**< Bulk tuning dump request (non-realtime) */
	MIDI_SYSEX_TUNING_BULK_DUMP = 0x01,						/**< Bulk tuning dump response (non-realtime) */
	MIDI_SYSEX_TUNING_NOTE_TUNE = 0x02,						/**< Tuning note change message (realtime) */
	MIDI_SYSEX_TUNING_BULK_DUMP_REQ_BANK = 0x03,	/**< Bulk tuning dump request (with bank, non-realtime) */
	MIDI_SYSEX_TUNING_BULK_DUMP_BANK = 0x04,			/**< Bulk tuning dump resonse (with bank, non-realtime) */
	MIDI_SYSEX_TUNING_OCTAVE_DUMP_1BYTE = 0x05,		/**< Octave tuning dump using 1 byte values (non-realtime) */
	MIDI_SYSEX_TUNING_OCTAVE_DUMP_2BYTE = 0x06,		/**< Octave tuning dump using 2 byte values (non-realtime) */
	MIDI_SYSEX_TUNING_NOTE_TUNE_BANK = 0x07,			/**< Tuning note change message (with bank, realtime/non-realtime) */
	MIDI_SYSEX_TUNING_OCTAVE_TUNE_1BYTE = 0x08,		/**< Octave tuning message using 1 byte values (realtime/non-realtime) */
	MIDI_SYSEX_TUNING_OCTAVE_TUNE_2BYTE = 0x09		/**< Octave tuning message using 2 byte values (realtime/non-realtime) */
};

/* General MIDI sub-ID #2 */
#define MIDI_SYSEX_GM_ON                0x01		/**< Enable GM mode */
#define MIDI_SYSEX_GM_OFF               0x02		/**< Disable GM mode */

enum driverStatus {
	MIDI_READY,
	MIDI_LISTENING,
	MIDI_DONE
};

/***************************************************************
 *
 *         TYPE DEFINITIONS & FUNCTION DECLARATIONS
 */

/* From ctype.h */
#define isascii(c)    (((c) & ~0x7f) == 0)

struct _MidiParserT;

struct _MidiParserT *newMidiParser(void);
void deleteMidiParser(struct _MidiParserT *parser);
MidiEventT *MidiParserParse(struct _MidiParserT *parser, unsigned char c);


/***************************************************************
 *
 *                   CONSTANTS & ENUM
 */


#define MAX_NUMBER_OF_TRACKS 128
#define MAX_NUMBER_OF_CHANNELS 16

/* General MIDI RPN event numbers (LSB, MSB = 0) */


/* MIDI SYSEX useful manufacturer values */

#define MIDI_SYSEX_DEVICE_ID_ALL        0x7F    /**< Device ID used in SYSEX messages to indicate all devices */

/* SYSEX sub-ID #1 which follows device ID */
#define MIDI_SYSEX_MIDI_TUNING_ID       0x08    /**< Sysex sub-ID #1 for MIDI tuning messages */
#define MIDI_SYSEX_GM_ID                0x09    /**< Sysex sub-ID #1 for General MIDI messages */
#define MIDI_SYSEX_GS_ID                0x42    /**< Model ID (GS) serving as sub-ID #1 for GS messages*/
#define MIDI_SYSEX_XG_ID                0x4C    /**< Model ID (XG) serving as sub-ID #1 for XG messages*/

/**
 * SYSEX tuning message IDs.
 */

/* General MIDI sub-ID #2 */
#define MIDI_SYSEX_GM_ON                0x01    /**< Enable GM mode */
#define MIDI_SYSEX_GM_OFF               0x02    /**< Disable GM mode */
#define MIDI_SYSEX_GM2_ON               0x03    /**< Enable GM2 mode */
#define MIDI_SYSEX_GS_DT1               0x12    /**< GS DT1 command */


/***************************************************************
 *
 *         TYPE DEFINITIONS & FUNCTION DECLARATIONS
 */


#define TrackEot(track)  ((track)->cur == NULL)

/* range of tempo values */
#define MIN_TEMPO_VALUE (1.0f)
#define MAX_TEMPO_VALUE (60000000.0f)
/* range of tempo multiplier values */
#define MIN_TEMPO_MULTIPLIER (0.001f)
#define MAX_TEMPO_MULTIPLIER (1000.0f)

/*
 * MidiFile
 */
typedef struct {
    const char *buffer;           /* Entire contents of MIDI file (borrowed) */
    int bufLen;                  /* Length of buffer, in bytes */
    int bufPos;                  /* Current read position in contents buffer */
    int eof;                      /* The "end of file" condition */
    int runningStatus;
    int c;
    int type;
    int ntracks;
    int usesSmpte;
    unsigned int smpteFps;
    unsigned int smpteRes;
    unsigned int division;       /* If uses_SMPTE == 0 then division is
				  ticks per beat (quarter-note) */
    double tempo;                /* Beats per second (SI rules =) */
    int tracklen;
    int trackpos;
    int eot;
    int varlen;
    int dtime;
} MidiFile;

#define MIDI_PARSER_MAX_DATA_SIZE 1024    /**< Maximum size of MIDI parameters/data (largest is SYSEX data) */

/* MidiParserT */
typedef struct _MidiParserT {
    unsigned char status;           /* Identifies the type of event, that is currently received ('Noteon', 'Pitch Bend' etc). */
    unsigned char channel;          /* The channel of the event that is received (in case of a channel event) */
    unsigned int nrBytes;          /* How many bytes have been read for the current event? */
    unsigned int nrBytesTotal;    /* How many bytes does the current event type include? */
    unsigned char data[MIDI_PARSER_MAX_DATA_SIZE]; /* The parameters or SYSEX data */
    MidiEventT event;        /* The event, that is returned to the MIDI driver. */
} MidiParserT;

#endif /* _MIDI_H */
