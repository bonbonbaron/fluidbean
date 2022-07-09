#include "chan.h"
#include "synth.h"
#include "midi.h"
#include "gen.h"

#define SETCC(_c,_n,_v)  _c->cc[_n] = _v

/* * newChannel */
struct _Channel *newChannel (Synthesizer * synth, int num) {
	struct _Channel *chan;

	chan = NEW (Channel);
	if (chan == NULL) 
		return NULL;

	chan->synth = synth;
	chan->channum = num;
	chan->presetP = NULL;

	channelInit (chan);
	channelInitCtrl (chan, 0);

	return chan;
}

void channelInit (Channel * chan) {
	chan->prognum = 0;  // prognum is the index of preset in the current bank number
	chan->banknum = 0;
	chan->sfontnum = 0;
  chan->presetP = &chan->synth->soundfontP->bankA[chan->banknum].presetA[chan->prognum];
	chan->interpMethod = INTERP_DEFAULT;
	chan->tuning = NULL;
	chan->nrpnSelect = 0;
	chan->nrpnActive = 0;
}

/*
  @param isAllCtrlOff if nonzero, only resets some controllers, according to
  http://www.midi.org/techspecs/rp15.php
*/
void channelInitCtrl (Channel * chan, int isAllCtrlOff) {
	int i;

	chan->channelPressure = 0;
	chan->pitchBend = 0x2000;		/* Range is 0x4000, pitch bend wheel starts in centered position */

	for (i = 0; i < GEN_LAST; i++) {
		chan->gen[i] = 0.0f;
		chan->genAbs[i] = 0;
	}

	if (isAllCtrlOff) {
		for (i = 0; i < ALL_SOUND_OFF; i++) {
			if (i >= EFFECTS_DEPTH1 && i <= EFFECTS_DEPTH5) 
				continue;
			if (i >= SOUND_CTRL1 && i <= SOUND_CTRL10) 
				continue;
			if (i == BANK_SELECT_MSB || i == BANK_SELECT_LSB || i == VOLUME_MSB ||
					i == VOLUME_LSB || i == PAN_MSB || i == PAN_LSB) 
				continue;

			SETCC (chan, i, 0);
		}
	} else 
		for (i = 0; i < 128; i++) 
			SETCC (chan, i, 0);

	/* Reset polyphonic key pressure on all voices */
  memset(chan->keyPressure, 0, sizeof(chan->keyPressure[0]) * 128);

	/* Set RPN controllers to NULL state */
	SETCC (chan, RPN_LSB, 127);
	SETCC (chan, RPN_MSB, 127);

	/* Set NRPN controllers to NULL state */
	SETCC (chan, NRPN_LSB, 127);
	SETCC (chan, NRPN_MSB, 127);

	/* Expression (MSB & LSB) */
	SETCC (chan, EXPRESSION_MSB, 127);
	SETCC (chan, EXPRESSION_LSB, 127);

	if (!isAllCtrlOff) {
		chan->pitchWheelSensitivity = 2;	/* two semi-tones */
		/* Just like panning, a value of 64 indicates no change for sound ctrls */
		for (i = SOUND_CTRL1; i <= SOUND_CTRL10; i++) 
			SETCC (chan, i, 64);

		/* Volume / initial attenuation (MSB & LSB) */
		SETCC (chan, VOLUME_MSB, 100);
		SETCC (chan, VOLUME_LSB, 0);

		/* Pan (MSB & LSB) */
		SETCC (chan, PAN_MSB, 64);
		SETCC (chan, PAN_LSB, 0);

		/* Reverb */
		/* SETCC(chan, EFFECTS_DEPTH1, 40); */
		/* Note: although XG standard specifies the default amount of reverb to
		   be 40, most people preferred having it at zero.
		   See http://lists.gnu.org/archive/html/fluid-dev/2009-07/msg00016.html */
	}
}

void channelReset (Channel * chan) {
	channelInit (chan);
	channelInitCtrl (chan, 0);
}

/*
 * deleteChannel
 */
void deleteChannel (Channel * chan) {
	FREE (chan);
}

/* channelCc */
void channelCc (Channel * chan, int num, int value) {
	chan->cc[num] = value;  // MB cc is either continous controller or controller change... Which is it?

	switch (num) {

	case SUSTAIN_SWITCH:
    if (value < 64) 
      synthDampVoices (chan->synth, chan->channum);
		break;

	case BANK_SELECT_MSB:
    if (chan->channum == 9 && chan->synth->settingsP->flags & DRUM_CHANNEL_IS_ACTIVE)
      return;

    chan->bankMsb = (unsigned char) (value & 0x7f);

    chan->banknum = (U32) (value & 0x7f);	/* KLE */
		break;

	case BANK_SELECT_LSB:
		{
			if (chan->channum == 9 && chan->synth->settingsP->flags & DRUM_CHANNEL_IS_ACTIVE)
        return;
			/* FIXME: according to the Downloadable Sounds II specification,
			   bit 31 should be set when we receive the message on channel
			   10 (drum channel) */
			chan->banknum = (((U32) value & 0x7f) + ((U32) chan->bankMsb << 7));
		}
		break;

	case ALL_NOTES_OFF:
		synthAllNotesOff (chan->synth, chan->channum);
		break;

	case ALL_SOUND_OFF:
		synthAllSoundsOff (chan->synth, chan->channum);
		break;

	case ALL_CTRL_OFF:
		channelInitCtrl (chan, 1);
		synthModulateVoicesAll (chan->synth, chan->channum);
		break;

	case DATA_ENTRY_MSB:
		{
			int data = (value << 7) + chan->cc[DATA_ENTRY_LSB];

			if (chan->nrpnActive) {	/* NRPN is active? */
				/* SontFont 2.01 NRPN Message (Sect. 9.6, p. 74)  */
				if ((chan->cc[NRPN_MSB] == 120) && (chan->cc[NRPN_LSB] < 100)) {
					if (chan->nrpnSelect < GEN_LAST) {
						float val = genScaleNrpn (chan->nrpnSelect, data);
            chan->synth->channel[chan->nrpnSelect]->genAbs[chan->nrpnSelect] = val;
					}

					chan->nrpnSelect = 0;	/* Reset to 0 */
				}
			} else if (chan->cc[RPN_MSB] == 0) {	/* RPN is active: MSB = 0? */
				switch (chan->cc[RPN_LSB]) {
				case RPN_PITCH_BEND_RANGE:
					channelPitchWheelSens (chan, value);	/* Set bend range in semitones */
					/* FIXME - Handle LSB? (Fine bend range in cents) */
					break;
				case RPN_CHANNEL_FINE_TUNE:	/* Fine tune is 14 bit over +/-1 semitone (+/- 100 cents, 8192 = center) */
          chan->synth->channel[chan->channum]->genAbs[GEN_FINETUNE] = (data - 8192) / 8192.0 * 100.0;
					break;
				case RPN_CHANNEL_COARSE_TUNE:	/* Coarse tune is 7 bit and in semitones (64 is center) */
          chan->synth->channel[chan->channum]->genAbs[GEN_COARSETUNE] = value - 64;
					break;
				case RPN_TUNING_PROGRAM_CHANGE:
					break;
				case RPN_TUNING_BANK_SELECT:
					break;
				case RPN_MODULATION_DEPTH_RANGE:
					break;
				}
			}

			break;
		}

	case NRPN_MSB:
		chan->cc[NRPN_LSB] = 0;
		chan->nrpnSelect = 0;
		chan->nrpnActive = 1;
		break;

	case NRPN_LSB:
		/* SontFont 2.01 NRPN Message (Sect. 9.6, p. 74)  */
		if (chan->cc[NRPN_MSB] == 120) {
			if (value == 100) {
				chan->nrpnSelect += 100;
			} else if (value == 101) {
				chan->nrpnSelect += 1000;
			} else if (value == 102) {
				chan->nrpnSelect += 10000;
			} else if (value < 100) {
				chan->nrpnSelect += value;
			}
		}

		chan->nrpnActive = 1;
		break;

	case RPN_MSB:
	case RPN_LSB:
		chan->nrpnActive = 0;
		break;

	default:
		synthModulateVoices (chan->synth, chan->channum, 1, num);  // 1 = isCc (what's cc?)
	}
}

/*
 * channelPressure
 */
void channelPressure (Channel * chan, int val) {
	chan->channelPressure = val;
	synthModulateVoices (chan->synth, chan->channum, 0, MOD_CHANNELPRESSURE);
}

/*
 * channelPitchBend
 */
void channelPitchBend (Channel * chan, int val) {
	chan->pitchBend = val;
	synthModulateVoices (chan->synth, chan->channum, 0, MOD_PITCHWHEEL);
}

/*
 * channelPitchWheelSens
 */
void channelPitchWheelSens (Channel * chan, int val) {
	chan->pitchWheelSensitivity = val;
	synthModulateVoices (chan->synth, chan->channum, 0, MOD_PITCHWHEELSENS);
}

