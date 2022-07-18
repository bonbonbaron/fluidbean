// MIDI channels each play one preset. It gets NRPN messages.
// NRPN messages are "non-registered parameter numbers". That means
// they are non-standard MIDI continuous controllers ("cc"). 
// NRPN is anything othan than basic pan, volume, etc. parameters 
// that not everybody has. Can be something weird.
// MIDI spec makes 128 continuous controllers availabe to each 
// channel.
//
// PITCH BEND: Although pitch bend is continuous, they don't count
// it as a CC because it needs a larger range than 0-255.
//
// Modulation all starts at the channel-level. 128 controllers (plus
// non-continuous ones) all connect to the level of entry: Channels.

#include "chan.h"
#include "synth.h"
#include "midi.h"
#include "gen.h"

#define SETCC(_c,_n,_v)  _c->cc[_n] = _v

struct _Channel* newChannel (Synthesizer* synthP, int chanNum) {
	struct _Channel *chanP = NEW (Channel);
	if (!chanP) 
		return NULL;

	chanP->synth = synthP;
	chanP->channum = chanNum;
	chanP->presetP = NULL;

	channelInit (chanP);
	channelInitCtrl (chanP, 0);

	return chanP;
}

void channelInit (Channel * chanP) {
	chanP->prognum = 0;  // prognum is the index of preset in the current bank number
	chanP->banknum = 0;
	chanP->sfontnum = 0;
  chanP->presetP = &chanP->synth->soundfontP->bankA[chanP->banknum].presetA[chanP->prognum];
	chanP->interpMethod = INTERP_DEFAULT;
	chanP->tuning = NULL;
	chanP->nrpnSelect = 0;
	chanP->nrpnActive = 0;
}

/* @param isAllCtrlOff if nonzero, only resets some controllers, according to
   http://www.midi.org/techspecs/rp15.php */
void channelInitCtrl (Channel * chanP, int isAllCtrlOff) {
	int i;

	chanP->channelPressure = 0;
	chanP->pitchBend = 0x2000;		/* Range is 0x4000, pitch bend wheel starts in centered position */

  memset(chanP->gen, 0, sizeof(chanP->gen[0]) * 128);
  memset(chanP->genAbs, 0, sizeof(chanP->genAbs[0]) * 128);

	if (isAllCtrlOff) {
		for (i = 0; i < ALL_SOUND_OFF; i++) {
      switch (i) {
        case EFFECTS_DEPTH1: case EFFECTS_DEPTH2: case EFFECTS_DEPTH3:
        case EFFECTS_DEPTH4: case EFFECTS_DEPTH5: case SOUND_CTRL1:
        case SOUND_CTRL2: case SOUND_CTRL3: case SOUND_CTRL4:
        case SOUND_CTRL5: case SOUND_CTRL6: case SOUND_CTRL7:
        case SOUND_CTRL8: case SOUND_CTRL9: case SOUND_CTRL10:
        case BANK_SELECT_LSB: case BANK_SELECT_MSB: case VOLUME_LSB:
        case VOLUME_MSB: case PAN_LSB: case PAN_MSB:
          continue;
          break;
        default:
          SETCC (chanP, i, 0);
          break;
      }
		}
	} else 
    memset(chanP->cc, 0, sizeof(chanP->cc[0]) * 128);

	/* Reset polyphonic key pressure on all voices */
  memset(chanP->keyPressure, 0, sizeof(chanP->keyPressure[0]) * 128);

	/* Set RPN controllers to NULL state */
	SETCC (chanP, RPN_LSB, 127);
	SETCC (chanP, RPN_MSB, 127);

	/* Set NRPN controllers to NULL state */
	SETCC (chanP, NRPN_LSB, 127);
	SETCC (chanP, NRPN_MSB, 127);

	/* Expression (MSB & LSB) */
	SETCC (chanP, EXPRESSION_MSB, 127);
	SETCC (chanP, EXPRESSION_LSB, 127);

	if (!isAllCtrlOff) {
		chanP->pitchWheelSensitivity = 2;	/* two semi-tones */
		/* Just like panning, a value of 64 indicates no change for sound ctrls */
		for (i = SOUND_CTRL1; i <= SOUND_CTRL10; i++) 
			SETCC (chanP, i, 64);

		/* Volume / initial attenuation (MSB & LSB) */
		SETCC (chanP, VOLUME_MSB, 100);
		SETCC (chanP, VOLUME_LSB, 0);

		/* Pan (MSB & LSB) */
		SETCC (chanP, PAN_MSB, 64);
		SETCC (chanP, PAN_LSB, 0);

		/* Reverb */
		/* SETCC(chanP, EFFECTS_DEPTH1, 40); */
		/* Note: although XG standard specifies the default amount of reverb to
		   be 40, most people preferred having it at zero.
		   See http://lists.gnu.org/archive/html/fluid-dev/2009-07/msg00016.html */
	}
}

void channelReset (Channel * chanP) {
	channelInit (chanP);
	channelInitCtrl (chanP, 0);
}

/*
 * deleteChannel
 */
void deleteChannel (Channel * chanP) {
	FREE (chanP);
}

/* channelCc */
void channelCc (Channel * chanP, int ccId, int value) {
	chanP->cc[ccId] = value;  // MB cc is either continous controller or controller change... Which is it?

	switch (ccId) {

	case SUSTAIN_SWITCH:
    if (value < 64) 
      synthDampVoices (chanP->synth, chanP->channum);
		break;

	case BANK_SELECT_MSB:
    if (chanP->channum == 9 && chanP->synth->settingsP->flags & DRUM_CHANNEL_IS_ACTIVE)
      return;

    chanP->bankMsb = (unsigned char) (value & 0x7f);

    chanP->banknum = (U32) (value & 0x7f);	/* KLE */
		break;

	case BANK_SELECT_LSB:
		{
			if (chanP->channum == 9 && chanP->synth->settingsP->flags & DRUM_CHANNEL_IS_ACTIVE)
        return;
			/* FIXME: according to the Downloadable Sounds II specification,
			   bit 31 should be set when we receive the message on channel
			   10 (drum channel) */
			chanP->banknum = (((U32) value & 0x7f) + ((U32) chanP->bankMsb << 7));
		}
		break;

	case ALL_NOTES_OFF:
		synthAllNotesOff (chanP->synth, chanP->channum);
		break;

	case ALL_SOUND_OFF:
		synthAllSoundsOff (chanP->synth, chanP->channum);
		break;

	case ALL_CTRL_OFF:
		channelInitCtrl (chanP, 1);
		synthModulateVoicesAll (chanP->synth, chanP->channum);
		break;

	case DATA_ENTRY_MSB:
		{
			int data = (value << 7) + chanP->cc[DATA_ENTRY_LSB];

			if (chanP->nrpnActive) {	/* NRPN is active? */
				/* SontFont 2.01 NRPN Message (Sect. 9.6, p. 74)  */
				if ((chanP->cc[NRPN_MSB] == 120) && (chanP->cc[NRPN_LSB] < 100)) {
					if (chanP->nrpnSelect < GEN_LAST) {
						float val = genScaleNrpn (chanP->nrpnSelect, data);
            chanP->synth->channel[chanP->nrpnSelect]->genAbs[chanP->nrpnSelect] = val;
					}

					chanP->nrpnSelect = 0;	/* Reset to 0 */
				}
			} else if (chanP->cc[RPN_MSB] == 0) {	/* RPN is active: MSB = 0? */
				switch (chanP->cc[RPN_LSB]) {
				case RPN_PITCH_BEND_RANGE:
					channelPitchWheelSens (chanP, value);	/* Set bend range in semitones */
					/* FIXME - Handle LSB? (Fine bend range in cents) */
					break;
				case RPN_CHANNEL_FINE_TUNE:	/* Fine tune is 14 bit over +/-1 semitone (+/- 100 cents, 8192 = center) */
          chanP->genAbs[GEN_FINETUNE] = (data - 8192) / 8192.0 * 100.0;
					break;
				case RPN_CHANNEL_COARSE_TUNE:	/* Coarse tune is 7 bit and in semitones (64 is center) */
          chanP->genAbs[GEN_COARSETUNE] = value - 64;
					break;
				case RPN_TUNING_PROGRAM_CHANGE:
				case RPN_TUNING_BANK_SELECT:
				case RPN_MODULATION_DEPTH_RANGE:
					break;
				}
			}

			break;
		}

	case NRPN_MSB:
		chanP->cc[NRPN_LSB] = 0;
		chanP->nrpnSelect = 0;
		chanP->nrpnActive = 1;
		break;

	case NRPN_LSB:
		/* SontFont 2.01 NRPN Message (Sect. 9.6, p. 74)  */
		if (chanP->cc[NRPN_MSB] == 120) {
			if (value == 100) {
				chanP->nrpnSelect += 100;
			} else if (value == 101) {
				chanP->nrpnSelect += 1000;
			} else if (value == 102) {
				chanP->nrpnSelect += 10000;
			} else if (value < 100) {
				chanP->nrpnSelect += value;
			}
		}

		chanP->nrpnActive = 1;
		break;

	case RPN_MSB:
	case RPN_LSB:
		chanP->nrpnActive = 0;
		break;

	default:
		synthModulateVoices (chanP->synth, chanP->channum, 1, ccId);  // 1 = isCc (what's cc?)
	}
}

/*
 * channelPressure
 */
void channelPressure (Channel * chanP, int val) {
	chanP->channelPressure = val;
	synthModulateVoices (chanP->synth, chanP->channum, 0, MOD_CHANNELPRESSURE);
}

/*
 * channelPitchBend
 */
void channelPitchBend (Channel * chanP, int val) {
	chanP->pitchBend = val;
	synthModulateVoices (chanP->synth, chanP->channum, 0, MOD_PITCHWHEEL);
}

/*
 * channelPitchWheelSens
 */
void channelPitchWheelSens (Channel * chanP, int val) {
	chanP->pitchWheelSensitivity = val;
	synthModulateVoices (chanP->synth, chanP->channum, 0, MOD_PITCHWHEELSENS);
}

