struct _Channel;
#ifndef _CHAN_H
#define _CHAN_H

#include "fluidbean.h"
#include "tuning.h"
#include "midi.h"
#include "enums.h"

#define NO_CHANNEL             0xff

/* Channel */
typedef struct _Channel {
	U8 channum;
	U8 sfontnum;
	U8 banknum;
	U8 prognum;
	Preset *presetP;
	struct _Synthesizer *synth;
	S8 keyPressure[128];
	S16 channelPressure;
	S16 pitchBend;
	S16 pitchWheelSensitivity;
	/* controller values */
  // WTF is "cc"?? Controller change, continuous controller, or channel controllers??
	S16 cc[128];  // MB TODO: change controller system for easy correspondence to new gen-mod scheme
	/* cached values of last MSB values of MSB/LSB controllers */
	U8 bankMsb;
	S32 interpMethod;
	tuningT *tuning;  /* the micro-tuning */
	/* NRPN system */
	S16 nrpnSelect;
	S16 nrpnActive;						/* 1 if data entry CCs are for NRPN, 0 if RPN */
	/* The values of the generators, set by NRPN messages, or by * synthSetGen(), are cached in the channel so they can be applied to future notes. They are copied to a voice's generators in voiceInit(), wihich calls genInit().  */
	S32 gen[GEN_LAST];
	// By default, the NRPN values are relative to the values of the generators set in the SoundFont. For example, if the NRPN specifies an attack of 100 msec then 100 msec will be added to the combined attack time of the sound font and the modulators.  However, it is useful to be able to specify the generator value absolutely, completely ignoring the generators of the sound font and the values of modulators. The genAbs field, is a boolean flag indicating whether the NRPN value is absolute or not. 
	S8 genAbs[GEN_LAST];
} Channel;

Channel *newChannel (struct _Synthesizer * synth, S32 num);
void deleteChannel (Channel * chan);
void channelInit (Channel * chan);
void channelInitCtrl (Channel * chan, S32 isAllCtrlOff);
void channelReset (Channel * chan);
void channelCc (Channel * chan, S32 ctrl, S32 val);
void channelPressure (Channel * chan, S32 val);
void channelPitchBend (Channel * chan, S32 val);
void channelPitchWheelSens (Channel * chan, S32 val);

#define channelGetKeyPressure(chan, key) \
  ((chan)->keyPressure[key])
#define channelSetKeyPressure(chan, key, val) \
  ((chan)->keyPressure[key] = (val))
#define channelSetTuning(_c, _t)        { (_c)->tuning = _t; }
#define channelHasTuning(_c)            ((_c)->tuning != NULL)
#define channelGetTuning(_c)            ((_c)->tuning)
#define channelSustained(_c)             ((_c)->cc[SUSTAIN_SWITCH] >= 64)
#define channelSetGen(_c, _n, _v, _a)   { (_c)->gen[_n] = _v; (_c)->genAbs[_n] = _a; }
#define channelGetGen(_c, _n)           ((_c)->gen[_n])
#define channelGetGenAbs(_c, _n)       ((_c)->genAbs[_n])

#define channelGetMinNoteLengthTicks(chan) \
  ((chan)->synth->minNoteLengthTicks)

#endif /* _CHAN_H */
