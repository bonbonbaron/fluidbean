#include "fluidbean.h"
#include "soundfont.h"
#include "chan.h"
#include "voice.h"
#include "conv.h"

void modClone (Modulator * mod, Modulator * src) {
	mod->dest = src->dest;
	mod->src1 = src->src1;
	mod->xformType1 = src->xformType1;
	mod->src2 = src->src2;
	mod->xformType2 = src->xformType2;
	mod->amount = src->amount;
}

void modSetSource1 (Modulator * mod, int src, int xformType) {
	mod->src1 = src;
	mod->xformType1 = xformType;
}

void modSetSource2 (Modulator * mod, int src, int xformType) {
	mod->src2 = src;
	mod->xformType2 = xformType;
}

realT modGetValue (Modulator * mod, Channel * chan, Voice * voice) {
	realT v1 = 0.0, v2 = 1.0;
	realT range1 = 127.0, range2 = 127.0;

	if (chan == NULL) {
		return 0.0f;
	}

	/* 'special treatment' for default controller
	 *  Reference: SF2.01 section 8.4.2
	 * */
	if ((mod->src2 == MOD_VELOCITY) &&
			(mod->src1 == MOD_VELOCITY) &&
			(mod->xformType1 == (MOD_GC | MOD_UNIPOLAR
											 | MOD_NEGATIVE | MOD_LINEAR)) &&
			(mod->xformType2 == (MOD_GC | MOD_UNIPOLAR
											 | MOD_POSITIVE | MOD_SWITCH)) &&
			(mod->dest == GEN_FILTERFC)) {
// S. Christian Collins' mod, to stop forcing velocity based filtering
/*
    if (voice->vel < 64){
      return (realT) mod->amount / 2.0;
    } else {
      return (realT) mod->amount * (127 - voice->vel) / 127;
    }
*/
		return 0;										// (realT) mod->amount / 2.0;
	}
// end S. Christian Collins' mod

	/* get the initial value of the first source */
	if (mod->src1 > 0) {
		if (mod->xformType1 & MOD_CC) {
			v1 = chan->cc[mod->src1];
		} else {										/* source 1 is one of the direct controllers */
			switch (mod->src1) {
			case MOD_NONE:			/* SF 2.01 8.2.1 item 0: src enum=0 => value is 1 */
				v1 = range1;
				break;
			case MOD_VELOCITY:
				v1 = voice->vel;
				break;
			case MOD_KEY:
				v1 = voice->key;
				break;
			case MOD_KEYPRESSURE:
				v1 = channelGetKeyPressure (chan, voice->key);
				break;
			case MOD_CHANNELPRESSURE:
				v1 = chan->channelPressure;
				break;
			case MOD_PITCHWHEEL:
				v1 = chan->pitchBend;
				range1 = 0x4000;
				break;
			case MOD_PITCHWHEELSENS:
				v1 = chan->pitchWheelSensitivity;
				break;
			default:
				v1 = 0.0;
			}
		}

		/* transform the input value */
		switch (mod->xformType1 & 0x0f) {
		case 0:										/* linear, unipolar, positive */
			v1 /= range1;
			break;
		case 1:										/* linear, unipolar, negative */
			v1 = 1.0f - v1 / range1;
			break;
		case 2:										/* linear, bipolar, positive */
			v1 = -1.0f + 2.0f * v1 / range1;
			break;
		case 3:										/* linear, bipolar, negative */
			v1 = 1.0f - 2.0f * v1 / range1;
			break;
		case 4:										/* concave, unipolar, positive */
			v1 = concave (v1);
			break;
		case 5:										/* concave, unipolar, negative */
			v1 = concave (127 - v1);
			break;
		case 6:										/* concave, bipolar, positive */
			v1 = (v1 > 64) ? concave (2 * (v1 - 64)) : -concave (2 * (64 - v1));
			break;
		case 7:										/* concave, bipolar, negative */
			v1 = (v1 > 64) ? -concave (2 * (v1 - 64)) : concave (2 * (64 - v1));
			break;
		case 8:										/* convex, unipolar, positive */
			v1 = convex (v1);
			break;
		case 9:										/* convex, unipolar, negative */
			v1 = convex (127 - v1);
			break;
		case 10:										/* convex, bipolar, positive */
			v1 = (v1 > 64) ? convex (2 * (v1 - 64)) : -convex (2 * (64 - v1));
			break;
		case 11:										/* convex, bipolar, negative */
			v1 = (v1 > 64) ? -convex (2 * (v1 - 64)) : convex (2 * (64 - v1));
			break;
		case 12:										/* switch, unipolar, positive */
			v1 = (v1 >= 64) ? 1.0f : 0.0f;
			break;
		case 13:										/* switch, unipolar, negative */
			v1 = (v1 >= 64) ? 0.0f : 1.0f;
			break;
		case 14:										/* switch, bipolar, positive */
			v1 = (v1 >= 64) ? 1.0f : -1.0f;
			break;
		case 15:										/* switch, bipolar, negative */
			v1 = (v1 >= 64) ? -1.0f : 1.0f;
			break;
		}
	} else {
		return 0.0;
	}

	/* no need to go further */
	if (v1 == 0.0f) {
		return 0.0f;
	}

	/* get the second input source */
	if (mod->src2 > 0) {
		if (mod->xformType2 & MOD_CC) {
			v2 = chan->cc[mod->src2];
		} else {
			switch (mod->src2) {
			case MOD_NONE:			/* SF 2.01 8.2.1 item 0: src enum=0 => value is 1 */
				v2 = range2;
				break;
			case MOD_VELOCITY:
				v2 = voice->vel;
				break;
			case MOD_KEY:
				v2 = voice->key;
				break;
			case MOD_KEYPRESSURE:
				v2 = channelGetKeyPressure (chan, voice->key);
				break;
			case MOD_CHANNELPRESSURE:
				v2 = chan->channelPressure;
				break;
			case MOD_PITCHWHEEL:
				v2 = chan->pitchBend;
				break;
			case MOD_PITCHWHEELSENS:
				v2 = chan->pitchWheelSensitivity;
				break;
			default:
				v1 = 0.0f;
			}
		}

		/* transform the second input value */
		switch (mod->xformType2 & 0x0f) {
		case 0:										/* linear, unipolar, positive */
			v2 /= range2;
			break;
		case 1:										/* linear, unipolar, negative */
			v2 = 1.0f - v2 / range2;
			break;
		case 2:										/* linear, bipolar, positive */
			v2 = -1.0f + 2.0f * v2 / range2;
			break;
		case 3:										/* linear, bipolar, negative */
			v2 = -1.0f + 2.0f * v2 / range2;
			break;
		case 4:										/* concave, unipolar, positive */
			v2 = concave (v2);
			break;
		case 5:										/* concave, unipolar, negative */
			v2 = concave (127 - v2);
			break;
		case 6:										/* concave, bipolar, positive */
			v2 = (v2 > 64) ? concave (2 * (v2 - 64)) : -concave (2 * (64 - v2));
			break;
		case 7:										/* concave, bipolar, negative */
			v2 = (v2 > 64) ? -concave (2 * (v2 - 64)) : concave (2 * (64 - v2));
			break;
		case 8:										/* convex, unipolar, positive */
			v2 = convex (v2);
			break;
		case 9:										/* convex, unipolar, negative */
			v2 = 1.0f - convex (v2);
			break;
		case 10:										/* convex, bipolar, positive */
			v2 = (v2 > 64) ? -convex (2 * (v2 - 64)) : convex (2 * (64 - v2));
			break;
		case 11:										/* convex, bipolar, negative */
			v2 = (v2 > 64) ? -convex (2 * (v2 - 64)) : convex (2 * (64 - v2));
			break;
		case 12:										/* switch, unipolar, positive */
			v2 = (v2 >= 64) ? 1.0f : 0.0f;
			break;
		case 13:										/* switch, unipolar, negative */
			v2 = (v2 >= 64) ? 0.0f : 1.0f;
			break;
		case 14:										/* switch, bipolar, positive */
			v2 = (v2 >= 64) ? 1.0f : -1.0f;
			break;
		case 15:										/* switch, bipolar, negative */
			v2 = (v2 >= 64) ? -1.0f : 1.0f;
			break;
		}
	} else {
		v2 = 1.0f;
	}

	/* it's as simple as that: */
	return (realT) mod->amount * v1 * v2;
}

/*
 * modNew
 */
Modulator *modNew () {
	Modulator *mod = NEW (Modulator);
	if (mod == NULL) 
		return NULL;
	return mod;
};

/*
 * modDelete
 */
void modDelete (Modulator * mod) {
	FREE (mod);
};

/*
 * ModulatorestIdentity
 */
/* Purpose:
 * Checks, if two modulators are identical.
 *  SF2.01 section 9.5.1 page 69, 'bullet' 3 defines 'identical'.
 */
int modTestIdentity (Modulator * mod1, Modulator * mod2) {
	if (mod1->dest != mod2->dest) {
		return 0;
	};
	if (mod1->src1 != mod2->src1) {
		return 0;
	};
	if (mod1->src2 != mod2->src2) {
		return 0;
	};
	if (mod1->xformType1 != mod2->xformType1) {
		return 0;
	}
	if (mod1->xformType2 != mod2->xformType2) {
		return 0;
	}
	return 1;
};
