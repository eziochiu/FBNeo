#include "gal.h"
#include "biquad.h"

UINT8 GalSoundType;
UINT8 GalSoundSubType = 0;
UINT8 HunchbksSoundIrqFire;

#define XTAL				18432000
#define RNG_RATE			(XTAL / 3)
#define NOISE_RATE			(XTAL / 3 / 192 / 2 / 2)
#define NOISE_LENGTH			(NOISE_RATE * 4)
#define NOISE_AMPLITUDE			(70 * 256)
#define SHOOT_VOLUME			0.50f
#define SHOOT_SEC			2
#define R41__ 				100000
#define R44__ 				10000
#define R45__ 				22000
#define R46__ 				10000
#define R47__ 				2200
#define R48__ 				2200
#define C25__ 				0.000001
#define C27__ 				0.00000001
#define C28__ 				0.000047
#define C29__ 				0.00000001
#define IC8L3_L 			0.2
#define IC8L3_H 			4.5
#define NOISE_L 			0.2
#define NOISE_H 			4.5
#define SHOOT_KEYON_TIME 		0.1
#define NE555_FM_ADJUST_RATE 		0.80
#define TOOTHSAW_LENGTH 		16
#define TOOTHSAW_VOLUME 		0.36f
#define 				TOOTHSAW_AMPLITUDE (64*256)
#define V(r0,r1) 			2 * TOOTHSAW_AMPLITUDE * (r0) / (r0 + r1) - TOOTHSAW_AMPLITUDE
#define STEPS 				16
#define LFO_VOLUME 			0.06f
#define MINFREQ 			(139 - 139 / 3)
#define MAXFREQ 			(139 + 139 / 3)

static INT16 *GalNoiseWave;
static INT16 *GalShootWave;
static UINT32 GalShootLength;
static UINT32 GalShootRate;
UINT8 GalLastPort2 = 0;
UINT8 GalShootEnable;
UINT8 GalNoiseEnable;
INT32 GalNoiseVolume;
INT32 GalNoiseHold;
double GalShootWavePos;
double GalNoiseWavePos;
double GalLfoWavePos[3];
INT32 GalPitch;
INT32 GalVol;
static INT32 GalCounter;
static INT32 GalCountDown;
INT32 GalLfoVolume[3];
double GalLfoFreq;
double GalLfoFreqFrameVar;
INT32 GalLfoBit[4];
static double GalGain;
static INT32 GalOutputDir;

static INT16 GalToneWave[4][TOOTHSAW_LENGTH];

static BIQ biquad_explode;
static BIQ biquad_explode2;

static const INT16 GalBackgroundWave[32] =
{
   0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000,
   0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000,
  -0x4000,-0x4000,-0x4000,-0x4000,-0x4000,-0x4000,-0x4000,-0x4000,
  -0x4000,-0x4000,-0x4000,-0x4000,-0x4000,-0x4000,-0x4000,-0x4000,
};

void GalaxianSoundSetRoute(double nVolume, INT32 nRouteDir)
{
	GalGain = nVolume;
	GalOutputDir = nRouteDir;
}

void harem_digitalker_data_write(UINT32, UINT32 data)
{
	digitalker_data_write(data);
}

void harem_digitalker_control_write(UINT32, UINT32 data)
{
	digitalker_cs_write((data & 1) >> 0);
	digitalker_cms_write((data & 2) >> 1);
	digitalker_wr_write((data & 4) >> 2);
}

UINT8 harem_digitalker_intr_read()
{
	return digitalker_intr_read();
}

void GalSoundReset()
{
	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_ZIGZAGAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_JUMPBUGAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_CHECKMANAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_CHECKMAJAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_FROGGERAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_MSHUTTLEAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_BONGOAY8910) {
		AY8910Reset(0);	
	}
	
	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_KONAMIAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_EXPLORERAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_SFXAY8910DAC || GalSoundType == GAL_SOUND_HARDWARE_TYPE_AD2083AY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_HUNCHBACKAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_FANTASTCAY8910) {
		AY8910Reset(0);
		AY8910Reset(1);
	}
	
	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_SCORPIONAY8910) {
		ZetOpen(1);
		AY8910Reset(0);
		AY8910Reset(1);
		AY8910Reset(2);
		digitalker_reset();
		ZetClose();
	}
	
	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_KINGBALLDAC || GalSoundType == GAL_SOUND_HARDWARE_TYPE_SFXAY8910DAC) {
		DACReset();
	}
	
	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_GALAXIAN || GalSoundType == GAL_SOUND_HARDWARE_TYPE_KINGBALLDAC) {
		GalLastPort2 = 0;
		GalShootEnable = 0;
		GalNoiseEnable = 0;
		GalNoiseVolume = 0;
		GalShootWavePos = 0;
		GalNoiseWavePos = 0;
		GalLfoWavePos[0] = GalLfoWavePos[1] = GalLfoWavePos[2] = 0;
		GalPitch = 0xff;
		GalVol = 0;
		GalCounter = 0;
		GalCountDown = 0;
		GalLfoVolume[0] = GalLfoVolume[1] = GalLfoVolume[2] = 0;
		GalLfoFreq = MAXFREQ;
		GalLfoFreqFrameVar = 0;
		GalLfoBit[0] = GalLfoBit[1] = GalLfoBit[2] = GalLfoBit[3] = 0;
	}
}

void GalSoundInit()
{
	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_ZIGZAGAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_CHECKMANAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_JUMPBUGAY8910) {
		AY8910Init(0, (GalSoundSubType == 1) ? 1536000 : 1789750, 0);
		AY8910SetAllRoutes(0, 0.50, BURN_SND_ROUTE_BOTH);
	}
	
	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_BONGOAY8910) {
		AY8910Init(0, 1789750, 0);
		AY8910SetPorts(0, &BongoDipSwitchRead, NULL, NULL, NULL);
		AY8910SetAllRoutes(0, 0.20, BURN_SND_ROUTE_BOTH);
	}
	
	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_CHECKMAJAY8910) {
		AY8910Init(0, 1620000, 0);
		AY8910SetPorts(0, &CheckmajPortARead, NULL, NULL, NULL);
		AY8910SetAllRoutes(0, 0.50, BURN_SND_ROUTE_BOTH);
	}
	
	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_MSHUTTLEAY8910) {
		// Port A Write - cclimber_sample_select_w
		AY8910Init(0, 18432000 / 3 / 4, 0);
		AY8910SetAllRoutes(0, 0.20, BURN_SND_ROUTE_BOTH);
		AY8910SetPorts(0, NULL, NULL, &cclimber_sample_num, NULL);
	}
	
	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_FROGGERAY8910) {
		AY8910Init(0, 14318000 / 8, 0);
		AY8910SetPorts(0, &KonamiSoundLatchRead, &FroggerSoundTimerRead, NULL, NULL);

		filter_rc_init(0, FLT_RC_LOWPASS, 1, 1, 1, 0, 0);
		filter_rc_init(1, FLT_RC_LOWPASS, 1, 1, 1, 0, 1);
		filter_rc_init(2, FLT_RC_LOWPASS, 1, 1, 1, 0, 1);

		filter_rc_set_src_gain(0, 0.75);
		filter_rc_set_src_gain(1, 0.75);
		filter_rc_set_src_gain(2, 0.75);
		
		filter_rc_set_route(0, 1.00, BURN_SND_ROUTE_BOTH);
		filter_rc_set_route(1, 1.00, BURN_SND_ROUTE_BOTH);
		filter_rc_set_route(2, 1.00, BURN_SND_ROUTE_BOTH);
	}	
	
	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_KONAMIAY8910) {
		AY8910Init(0, 14318000 / 8, 0);
		AY8910Init(1, 14318000 / 8, 1);
		AY8910SetPorts(1, &KonamiSoundLatchRead, &KonamiSoundTimerRead, NULL, NULL);
		
		filter_rc_init(0, FLT_RC_LOWPASS, 1, 1, 1, 0, 0);
		filter_rc_init(1, FLT_RC_LOWPASS, 1, 1, 1, 0, 1);
		filter_rc_init(2, FLT_RC_LOWPASS, 1, 1, 1, 0, 1);
		filter_rc_init(3, FLT_RC_LOWPASS, 1, 1, 1, 0, 1);
		filter_rc_init(4, FLT_RC_LOWPASS, 1, 1, 1, 0, 1);
		filter_rc_init(5, FLT_RC_LOWPASS, 1, 1, 1, 0, 1);
		
		filter_rc_set_src_gain(0, 0.25);
		filter_rc_set_src_gain(1, 0.25);
		filter_rc_set_src_gain(2, 0.25);
		filter_rc_set_src_gain(3, 0.25);
		filter_rc_set_src_gain(4, 0.25);
		filter_rc_set_src_gain(5, 0.25);
		
		filter_rc_set_route(0, 1.00, BURN_SND_ROUTE_BOTH);
		filter_rc_set_route(1, 1.00, BURN_SND_ROUTE_BOTH);
		filter_rc_set_route(2, 1.00, BURN_SND_ROUTE_BOTH);
		filter_rc_set_route(3, 1.00, BURN_SND_ROUTE_BOTH);
		filter_rc_set_route(4, 1.00, BURN_SND_ROUTE_BOTH);
		filter_rc_set_route(5, 1.00, BURN_SND_ROUTE_BOTH);
	}
	
	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_EXPLORERAY8910) {
		AY8910Init(0, 14318000 / 8, 0);
		AY8910Init(1, 14318000 / 8, 1);
		AY8910SetPorts(0, &KonamiSoundTimerRead, NULL, NULL, NULL);
		AY8910SetPorts(1, &KonamiSoundLatchRead, NULL, NULL, NULL);
		AY8910SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);
		AY8910SetAllRoutes(1, 0.25, BURN_SND_ROUTE_BOTH);
	}
	
	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_SCORPIONAY8910) {
		AY8910Init(0, 14318000 / 8, 0);
		AY8910Init(1, 14318000 / 8, 1);
		AY8910Init(2, 14318000 / 8, 1);
		AY8910SetPorts(1, &KonamiSoundLatchRead, &KonamiSoundTimerRead, NULL, NULL);
		AY8910SetPorts(2, NULL, NULL, harem_digitalker_data_write, harem_digitalker_control_write);
		AY8910SetAllRoutes(0, 0.35, BURN_SND_ROUTE_BOTH);
		AY8910SetAllRoutes(1, 0.35, BURN_SND_ROUTE_BOTH);
		AY8910SetAllRoutes(2, 0.35, BURN_SND_ROUTE_BOTH);

		digitalker_init(digitalk_rom, GalSndROMSize, 4000000, ZetTotalCycles, 1789750, 1);
		digitalker_volume(0.30);
	}
	
	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_AD2083AY8910) {
		AY8910Init(0, 14318000 / 8, 0);
		AY8910Init(1, 14318000 / 8, 1);
		AY8910SetPorts(0, &KonamiSoundTimerRead, NULL, NULL, NULL);
		AY8910SetPorts(1, &KonamiSoundLatchRead, NULL, NULL, NULL);
		AY8910SetAllRoutes(0, 1.00, BURN_SND_ROUTE_BOTH);
		AY8910SetAllRoutes(1, 1.00, BURN_SND_ROUTE_BOTH);
	}
	
	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_FANTASTCAY8910) {
		AY8910Init(0, 1789750, 0);
		AY8910Init(1, 1789750, 1);
		AY8910SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);
		AY8910SetAllRoutes(1, 0.25, BURN_SND_ROUTE_BOTH);
	}
	
	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_SFXAY8910DAC) {
		AY8910Init(0, 14318000 / 8, 0);
		AY8910Init(1, 14318000 / 8, 1);
		AY8910SetPorts(0, NULL, NULL, &SfxSoundLatch2Write, &SfxSampleControlWrite);
		AY8910SetPorts(1, &KonamiSoundLatchRead, &KonamiSoundTimerRead, NULL, NULL);
		AY8910SetAllRoutes(0, 0.10, BURN_SND_ROUTE_BOTH);
		AY8910SetAllRoutes(1, 0.10, BURN_SND_ROUTE_BOTH);

		filter_rc_init(0, FLT_RC_LOWPASS, 1, 1, 1, 0, 0);
		filter_rc_init(1, FLT_RC_LOWPASS, 1, 1, 1, 0, 1);
		filter_rc_init(2, FLT_RC_LOWPASS, 1, 1, 1, 0, 1);
		filter_rc_init(3, FLT_RC_LOWPASS, 1, 1, 1, 0, 1);
		filter_rc_init(4, FLT_RC_LOWPASS, 1, 1, 1, 0, 1);
		filter_rc_init(5, FLT_RC_LOWPASS, 1, 1, 1, 0, 1);
		
		filter_rc_set_src_gain(0, 0.50);
		filter_rc_set_src_gain(1, 0.50);
		filter_rc_set_src_gain(2, 0.50);
		filter_rc_set_src_gain(3, 0.50);
		filter_rc_set_src_gain(4, 0.50);
		filter_rc_set_src_gain(5, 0.50);
		
		filter_rc_set_route(0, 0.10, BURN_SND_ROUTE_BOTH);
		filter_rc_set_route(1, 0.10, BURN_SND_ROUTE_BOTH);
		filter_rc_set_route(2, 0.10, BURN_SND_ROUTE_BOTH);
		filter_rc_set_route(3, 0.10, BURN_SND_ROUTE_BOTH);
		filter_rc_set_route(4, 0.10, BURN_SND_ROUTE_BOTH);
		filter_rc_set_route(5, 0.10, BURN_SND_ROUTE_BOTH);
		
		DACInit(0, 0, 1, SfxSyncDAC);
		DACSetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);
	}
	
	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_KINGBALLDAC) {
		DACInit(0, 0, 1, KingballSyncDAC);
		DACSetRoute(0, 0.25, BURN_SND_ROUTE_BOTH);
	}
	
	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_HEXPOOLASN76496) {
		SN76496Init(0, 18432000 / 3 / 2, 0);
		SN76496SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);
	}
	
	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_RACKNROLSN76496) {
		SN76496Init(0, 18432000 / 3 / 2, 0);
		SN76496Init(1, 18432000 / 3 / 2, 1);
		SN76496Init(2, 18432000 / 3 / 2, 1);
		SN76496SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);
		SN76496SetRoute(1, 1.00, BURN_SND_ROUTE_BOTH);
		SN76496SetRoute(2, 1.00, BURN_SND_ROUTE_BOTH);
	}
	
	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_HUNCHBACKAY8910) {
		AY8910Init(0, 14318000 / 8, 0);
		AY8910Init(1, 14318000 / 8, 1);
		AY8910SetPorts(1, &KonamiSoundLatchRead, &HunchbksSoundTimerRead, NULL, NULL);
		
		AY8910SetAllRoutes(0, 0.20, BURN_SND_ROUTE_BOTH);
		AY8910SetAllRoutes(1, 0.20, BURN_SND_ROUTE_BOTH);
	}	 
	
	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_GALAXIAN || GalSoundType == GAL_SOUND_HARDWARE_TYPE_KINGBALLDAC) {
		GalShootEnable = 0;
		GalShootWavePos = 0;		
		INT32 CountDown, Generator, Bit1, Bit2;

		GalNoiseWave = (INT16*)BurnMalloc(NOISE_LENGTH * sizeof(INT16));

		GalShootRate = 22050;
		GalShootLength = SHOOT_SEC * GalShootRate;
		GalShootWave = (INT16*)BurnMalloc((GalShootLength+1) * sizeof(INT16));
	
		Generator = 0;
		CountDown = NOISE_RATE / 2;
		for (INT32 i = 0; i < NOISE_LENGTH; i++) {
			CountDown -= RNG_RATE;
			while (CountDown < 0) {
				Generator <<= 1;
				Bit1 = (~Generator >> 17) & 1;
				Bit2 = (Generator >> 5) & 1;
				if (Bit1 ^ Bit2) Generator |= 1;
				CountDown += NOISE_RATE;
			}
			GalNoiseWave[i] = ((Generator >> 17) & 1) ? NOISE_AMPLITUDE : -NOISE_AMPLITUDE;
		}	

		double v  = 5.0;
		double vK = (GalShootRate) ? exp(-1 / (R41__ * C25__) / GalShootRate) : 0;
		double IC8L3 = IC8L3_L;
		INT32 IC8Lcnt = (INT32)(SHOOT_KEYON_TIME * GalShootRate);
		double c28v = IC8L3_H - (IC8L3_H - (NOISE_H + NOISE_L) / 2) / (R46__ + R47__ + R48__) * R47__;
		double c28K = (GalShootRate) ? exp(-1 / (22000 * 0.000047 ) / GalShootRate) : 0;
		double c29v  = IC8L3_H - (IC8L3_H - (NOISE_H + NOISE_L) / 2) / (R46__ + R47__ + R48__) * (R47__ + R48__);
		double c29K1 = (GalShootRate) ? exp(-1 / (22000  * 0.00000001 ) / GalShootRate) : 0;
		double c29K2 = (GalShootRate) ? exp(-1 / (100000 * 0.00000001 ) / GalShootRate) : 0;
		double ne555cnt = 0;
		double ne555step = (GalShootRate) ? ((1.44 / ((R44__ + R45__ * 2) * C27__)) / GalShootRate) : 0;
		double ne555duty = (double)(R44__ + R45__) / (R44__ + R45__ * 2);
		double ne555sr;
		double ncnt  = 0.0;
		double nstep = (GalShootRate) ? ((double)NOISE_RATE / GalShootRate) : 0;
		double noise_sh2;

		for (UINT32 i = 0; i < GalShootLength; i++) {
			noise_sh2 = GalNoiseWave[(INT32)ncnt % NOISE_LENGTH] == NOISE_AMPLITUDE ? NOISE_H : NOISE_L;
			ncnt += nstep;
			ne555sr = c29v * NE555_FM_ADJUST_RATE / (5.0 * 2 / 3);
			ne555cnt += ne555step;
			if (ne555cnt >= ne555sr) ne555cnt -= ne555sr;
			if (ne555cnt < ne555sr * ne555duty) {
				GalShootWave[i] = (INT16)(v / 5 * 0x7fff);
				if(IC8L3 == IC8L3_H) v *= vK;
			} else {
				GalShootWave[i] = 0;
			}
			c28v += (IC8L3 - c28v) - (IC8L3 - c28v) * c28K;
			c28v += (c29v - c28v) - (c29v - c28v) * c28K;
			c29v += (c28v - c29v) - (c28v - c29v) * c29K1;
			c29v += (noise_sh2 - c29v) - (noise_sh2 - c29v) * c29K2;
			if(IC8L3 == IC8L3_L && --IC8Lcnt == 0) IC8L3 = IC8L3_H;
		}
		
		memset(GalToneWave, 0, sizeof(GalToneWave));

		for (UINT32 i = 0; i < TOOTHSAW_LENGTH; i++ ) {
			double r0a = 1.0 / 1e12, r1a = 1.0 / 1e12;
			double r0b = 1.0 / 1e12, r1b = 1.0 / 1e12;

			if (i & 1) {
				r1a += 1.0 / 33000;
				r1b += 1.0 / 33000;
			} else {
				r0a += 1.0 / 33000;
				r0b += 1.0 / 33000;
			}
			if (i & 4) {
				r1a += 1.0 / 22000;
				r1b += 1.0 / 22000;
			} else {
				r0a += 1.0 / 22000;
				r0b += 1.0 / 22000;
			}
			GalToneWave[0][i] = (INT16)(V(1.0 / r0a, 1.0 / r1a));

			if (i & 4) {
				r1a += 1.0 / 10000;
			} else {
				r0a += 1.0 / 10000;
			}
			GalToneWave[1][i] = (INT16)(V(1.0 / r0a, 1.0 / r1a));

			if (i & 8) {
				r1b += 1.0 / 15000;
			} else {
				r0b += 1.0 / 15000;
			}
			GalToneWave[2][i] = (INT16)(V(1.0 / r0b, 1.0 / r1b));
			
			if (i & 4) {
				r0b += 1.0 / 10000;
			} else {
				r1b += 1.0 / 10000;
			}
			GalToneWave[3][i] = (INT16)(V(1.0 / r0b, 1.0 / r1b));
		}
		
		GalPitch = 0xff;
		GalVol = 0;
		
		GalLfoFreq = MAXFREQ;
		GalLfoFreqFrameVar = 0;
		
		GalGain = 4.00;
		GalOutputDir = BURN_SND_ROUTE_BOTH;

		biquad_explode.init(FILT_LOWSHELF, nBurnSoundRate, 150.00, 1.00, 10.0);
		biquad_explode2.init(FILT_LOWPASS, nBurnSoundRate, 550.00, 1.00, 10.0);
	}
}

void GalSoundExit()
{
	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_ZIGZAGAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_JUMPBUGAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_CHECKMANAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_CHECKMAJAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_FROGGERAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_BONGOAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_MSHUTTLEAY8910) {
		AY8910Exit(0);
	}
	
	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_KONAMIAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_EXPLORERAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_SFXAY8910DAC || GalSoundType == GAL_SOUND_HARDWARE_TYPE_AD2083AY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_HUNCHBACKAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_FANTASTCAY8910) {
		AY8910Exit(0);
		AY8910Exit(1);
	}
	
	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_SCORPIONAY8910) {
		AY8910Exit(0);
		AY8910Exit(1);
		AY8910Exit(2);
		digitalker_exit();
	}
	
	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_KINGBALLDAC || GalSoundType == GAL_SOUND_HARDWARE_TYPE_SFXAY8910DAC) {
		DACExit();
	}
	
	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_RACKNROLSN76496 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_HEXPOOLASN76496) {
		SN76496Exit();
	}
	
	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_KONAMIAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_FROGGERAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_SFXAY8910DAC) {
		filter_rc_exit();
	}

	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_GALAXIAN || GalSoundType == GAL_SOUND_HARDWARE_TYPE_KINGBALLDAC) {
		biquad_explode.exit();
		biquad_explode2.exit();
	}

	BurnFree(GalNoiseWave);
	BurnFree(GalShootWave);

	HunchbksSoundIrqFire = 0;
	GalShootLength = 0;
	GalShootRate = 0;
	GalLastPort2 = 0;
	GalShootEnable = 0;
	GalNoiseEnable = 0;
	GalNoiseVolume = 0;
	GalShootWavePos = 0;
	GalNoiseWavePos = 0;
	GalLfoWavePos[0] = GalLfoWavePos[1] = GalLfoWavePos[2] = 0;
	GalPitch = 0;
	GalVol = 0;
	GalCounter = 0;
	GalCountDown = 0;
	GalLfoVolume[0] = GalLfoVolume[1] = GalLfoVolume[2] = 0;
	GalLfoFreq = 0;
	GalLfoFreqFrameVar = 0;
	GalLfoBit[0] = GalLfoBit[1] = GalLfoBit[2] = GalLfoBit[3] = 0;
	memset(GalToneWave, 0, sizeof(GalToneWave));
	GalSoundSubType = 0;
}

void GalSoundScan(INT32 nAction, INT32 *pnMin)
{
	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_GALAXIAN || GalSoundType == GAL_SOUND_HARDWARE_TYPE_KINGBALLDAC) {
		SCAN_VAR(GalLastPort2);
		SCAN_VAR(GalShootEnable);
		SCAN_VAR(GalNoiseEnable);
		SCAN_VAR(GalNoiseVolume);
		SCAN_VAR(GalNoiseHold);
		SCAN_VAR(GalShootWavePos);
		SCAN_VAR(GalNoiseWavePos);
		SCAN_VAR(GalLfoWavePos);
		SCAN_VAR(GalPitch);
		SCAN_VAR(GalVol);
		SCAN_VAR(GalCounter);
		SCAN_VAR(GalCountDown);
		SCAN_VAR(GalLfoVolume);
		SCAN_VAR(GalLfoFreq);
		SCAN_VAR(GalLfoFreqFrameVar);
		SCAN_VAR(GalLfoBit);
	}

	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_FROGGERAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_KONAMIAY8910 ||
		GalSoundType == GAL_SOUND_HARDWARE_TYPE_EXPLORERAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_ZIGZAGAY8910 ||
		GalSoundType == GAL_SOUND_HARDWARE_TYPE_SCORPIONAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_FROGGERAY8910 ||
		GalSoundType == GAL_SOUND_HARDWARE_TYPE_HUNCHBACKAY8910) {
		ppi8255_scan();
	}

	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_SCORPIONAY8910) {
		digitalker_scan(nAction, pnMin);
	}

	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_MSHUTTLEAY8910) {
		cclimber_sample_scan();
	}

	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_ZIGZAGAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_JUMPBUGAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_CHECKMANAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_CHECKMAJAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_FROGGERAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_BONGOAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_KONAMIAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_EXPLORERAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_SFXAY8910DAC || GalSoundType == GAL_SOUND_HARDWARE_TYPE_AD2083AY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_SCORPIONAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_HUNCHBACKAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_FANTASTCAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_MSHUTTLEAY8910) {
		AY8910Scan(nAction, pnMin);
	}

	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_KINGBALLDAC || GalSoundType == GAL_SOUND_HARDWARE_TYPE_SFXAY8910DAC) {
		DACScan(nAction, pnMin);
	}

	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_RACKNROLSN76496) {
		SN76496Scan(nAction, pnMin);
	}
}

// AY8910 Port Handlers
UINT8 CheckmajPortARead(UINT32)
{
	return GalSoundLatch;
}

UINT8 BongoDipSwitchRead(UINT32)
{
	return GalInput[3] | GalDip[3];
}

UINT8 KonamiSoundLatchRead(UINT32)
{
	ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
	return GalSoundLatch;
}

UINT8 KonamiSoundTimerRead(UINT32)
{
	INT32 nActiveCPU = ZetGetActive();
	UINT32 Cycles;
	
	if (nActiveCPU == 1) {
		Cycles = (ZetTotalCycles() * 8) % (UINT64)(16 * 16 * 2 * 8 * 5 * 2);
	} else {
		ZetClose();
		ZetOpen(1);
		Cycles = (ZetTotalCycles() * 8) % (UINT64)(16 * 16 * 2 * 8 * 5 * 2);
		ZetClose();
		ZetOpen(nActiveCPU);
	}
	
	UINT8 HiBit = 0;
	
	if (Cycles >= 16 * 16 * 2 * 8 * 5) {
		HiBit = 1;
		Cycles -= 16 * 16 * 2 * 8 * 5;
	}
	
	return (HiBit << 7) | (BIT(Cycles, 14) << 6) | (BIT(Cycles, 13) << 5) | (BIT(Cycles, 11) << 4) | 0x0e;
}

UINT8 FroggerSoundTimerRead(UINT32)
{
	UINT8 KonamiValue = KonamiSoundTimerRead(0);
	return BITSWAP08(KonamiValue, 7, 6, 3, 4, 5, 2, 1, 0);
}

UINT8 HunchbksSoundTimerRead(UINT32)
{
	INT32 nActiveCPU = ZetGetActive();
	UINT32 Cycles;
	
	if (nActiveCPU == 0) {
		Cycles = (ZetTotalCycles() * 8) % (UINT64)(16 * 16 * 2 * 8 * 5 * 2);
	} else {
		ZetOpen(0);
		Cycles = (ZetTotalCycles() * 8) % (UINT64)(16 * 16 * 2 * 8 * 5 * 2);
		ZetClose();
	}
	
	UINT8 HiBit = 0;
	
	if (Cycles >= 16 * 16 * 2 * 8 * 5) {
		HiBit = 1;
		Cycles -= 16 * 16 * 2 * 8 * 5;
	}
	
	return (HiBit << 7) | (BIT(Cycles, 14) << 6) | (BIT(Cycles, 13) << 5) | (BIT(Cycles, 11) << 4) | 0x0e;
}

void KonamiSoundLatchWrite(UINT8 d)
{
	GalSoundLatch = d;
}

void KonamiSoundControlWrite(UINT8 d)
{
	UINT8 Old = KonamiSoundControl;
	KonamiSoundControl = d;

	if ((Old & 0x08) && !(d & 0x08)) {
		INT32 nActiveCPU = ZetGetActive();
		
		if (nActiveCPU == 1) {
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		} else {
			ZetClose();
			ZetOpen(1);
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
			ZetClose();
			ZetOpen(nActiveCPU);
		}
	}

	/* bit 4 is sound disable */
//	sound_global_enable(~data & 0x10);
}

void HunchbksSoundControlWrite(UINT8 d)
{
	UINT8 Old = KonamiSoundControl;
	KonamiSoundControl = d;
	
	HunchbksSoundIrqFire = 0;

	if ((Old & 0x08) && !(d & 0x08)) {
		HunchbksSoundIrqFire = 1;
	}

	/* bit 4 is sound disable */
//	sound_global_enable(~data & 0x10);
}

void SfxSoundLatch2Write(UINT32, UINT32 d)
{
	GalSoundLatch2 = d & 0xff;
}

void SfxSampleControlWrite(UINT32, UINT32 d)
{
	UINT8 Old = SfxSampleControl;
	
	d &= 0xff;
	SfxSampleControl = d;

	if ((Old & 0x01) && !(d & 0x01)) {
		INT32 nActiveCPU = ZetGetActive();
		
		if (nActiveCPU == 2) {
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
			nGalCyclesDone[2] += ZetRun(100);
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			
		} else {
			ZetClose();
			ZetOpen(2);
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
			nGalCyclesDone[2] += ZetRun(100);
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			ZetClose();
			ZetOpen(nActiveCPU);
		}
	}
}

// Frogger Sound CPU Memory Map
static void filter_w(INT32 num, UINT8 d)
{
	INT32 C;
	
	C = 0;
	if (d & 1) C += 220000;	/* 220000pF = 0.220uF */
	if (d & 2) C +=  47000;	/*  47000pF = 0.047uF */
	filter_rc_set_RC(num, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(C));
}

UINT8 __fastcall FroggerSoundZ80Read(UINT16 a)
{
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 #2 Read => %04X\n"), a);
		}
	}

	return 0xff;
}

void __fastcall FroggerSoundZ80Write(UINT16 a, UINT8 d)
{
	if (a >= 0x6000 && a <= 0x6fff) {
		INT32 Offset = a & 0xfff;
		filter_w(0, (Offset >>  6) & 3);
		filter_w(1, (Offset >>  8) & 3);
		filter_w(2, (Offset >> 10) & 3);
		return;
	}
	
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 #2 Write => %04X, %02X\n"), a, d);
		}
	}
}

UINT8 __fastcall FroggerSoundZ80PortRead(UINT16 a)
{
	a &= 0xff;
	
	UINT8 Result = 0xff;
	if (a & 0x40) Result &= AY8910Read(0);
	return Result;
}

void __fastcall FroggerSoundZ80PortWrite(UINT16 a, UINT8 d)
{
	a &= 0xff;
	
	if (a & 0x40) {
		AY8910Write(0, 1, d);
	} else {
		if (a & 0x80) {
			AY8910Write(0, 0, d);
		}
	}
}

void FroggerSoundNoEncryptionInit()
{
	ZetOpen(1);
	ZetSetReadHandler(FroggerSoundZ80Read);
	ZetSetWriteHandler(FroggerSoundZ80Write);
	ZetSetInHandler(FroggerSoundZ80PortRead);
	ZetSetOutHandler(FroggerSoundZ80PortWrite);
	ZetMapArea(0x0000, GalZ80Rom2Size - 1, 0, GalZ80Rom2);
	ZetMapArea(0x0000, GalZ80Rom2Size - 1, 2, GalZ80Rom2);
	ZetMapArea(0x4000, 0x43ff, 0, GalZ80Ram2);
	ZetMapArea(0x4000, 0x43ff, 1, GalZ80Ram2);
	ZetMapArea(0x4000, 0x43ff, 2, GalZ80Ram2);
	ZetClose();
	
	nGalCyclesTotal[1] = (14318000 / 8) / 60;
}

void FroggerSoundInit()
{
	FroggerSoundNoEncryptionInit();
	
	for (UINT32 Offset = 0; Offset < 0x0800; Offset++) GalZ80Rom2[Offset] = BITSWAP08(GalZ80Rom2[Offset], 7, 6, 5, 4, 3, 2, 0, 1);
}

// Konami Sound CPU Memory Map
UINT8 __fastcall KonamiSoundZ80Read(UINT16 a)
{
	if (a < 0x8000) {
		// unmapped read, returning 0xff here causes calipso soundcpu to hang @ end of level.
		return 0x00;
	}

	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 #2 Read => %04X\n"), a);
		}
	}

	return 0xff;
}

void __fastcall KonamiSoundZ80Write(UINT16 a, UINT8 d)
{
	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_KONAMIAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_FROGGERAY8910 || GalSoundType == GAL_SOUND_HARDWARE_TYPE_SFXAY8910DAC) {
		if (a >= 0x9000 && a <= 0x9fff) {
			INT32 Offset = a & 0xfff;
			filter_w(0, (Offset >>  0) & 3);
			filter_w(1, (Offset >>  2) & 3);
			filter_w(2, (Offset >>  4) & 3);
			filter_w(3, (Offset >>  6) & 3);
			filter_w(4, (Offset >>  8) & 3);
			filter_w(5, (Offset >> 10) & 3);
			return;
		}
	}

	if (GalSoundType == GAL_SOUND_HARDWARE_TYPE_SCORPIONAY8910) {
		if (a >= 0xa000 && a <= 0xafff) {
			// no filters? (harem)
			return;
		}
		if (a >= 0x9000 && a <= 0x9fff) {
			// no filters? (scorpion)
			return;
		}
	}

	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 #2 Write => %04X, %02X\n"), a, d);
		}
	}
}

UINT8 __fastcall KonamiSoundZ80PortRead(UINT16 a)
{
	a &= 0xff;
	
	UINT8 Result = 0xff;
	if (a & 0x20) Result &= AY8910Read(0);
	if (a & 0x80) Result &= AY8910Read(1);
	return Result;
}

void __fastcall KonamiSoundZ80PortWrite(UINT16 a, UINT8 d)
{
	a &= 0xff;
	
	if (a & 0x10) {
		AY8910Write(0, 0, d);
	} else {
		if (a & 0x20) {
			AY8910Write(0, 1, d);
		}
	}
	
	if (a & 0x40) {
		AY8910Write(1, 0, d);
	} else {
		if (a & 0x80) {
			AY8910Write(1, 1, d);
		}
	}
}

void KonamiSoundInit()
{	
	ZetOpen(1);
	ZetSetReadHandler(KonamiSoundZ80Read);
	ZetSetWriteHandler(KonamiSoundZ80Write);
	ZetSetInHandler(KonamiSoundZ80PortRead);
	ZetSetOutHandler(KonamiSoundZ80PortWrite);
	ZetMapArea(0x0000, GalZ80Rom2Size - 1, 0, GalZ80Rom2);
	ZetMapArea(0x0000, GalZ80Rom2Size - 1, 2, GalZ80Rom2);
	ZetMapArea(0x8000, 0x83ff, 0, GalZ80Ram2);
	ZetMapArea(0x8000, 0x83ff, 1, GalZ80Ram2);
	ZetMapArea(0x8000, 0x83ff, 2, GalZ80Ram2);
	ZetClose();
	
	nGalCyclesTotal[1] = (14318000 / 8) / 60;
}

void HunchbksSoundInit()
{	
	ZetOpen(0);
	ZetSetReadHandler(KonamiSoundZ80Read);
	ZetSetWriteHandler(KonamiSoundZ80Write);
	ZetSetInHandler(KonamiSoundZ80PortRead);
	ZetSetOutHandler(KonamiSoundZ80PortWrite);
	ZetMapArea(0x0000, GalZ80Rom1Size - 1, 0, GalZ80Rom1);
	ZetMapArea(0x0000, GalZ80Rom1Size - 1, 2, GalZ80Rom1);
	ZetMapArea(0x8000, 0x83ff, 0, GalZ80Ram1 + 0x400);
	ZetMapArea(0x8000, 0x83ff, 1, GalZ80Ram1 + 0x400);
	ZetMapArea(0x8000, 0x83ff, 2, GalZ80Ram1 + 0x400);
	ZetMapArea(0x8400, 0x87ff, 0, GalZ80Ram1 + 0x400);
	ZetMapArea(0x8400, 0x87ff, 1, GalZ80Ram1 + 0x400);
	ZetMapArea(0x8400, 0x87ff, 2, GalZ80Ram1 + 0x400);
	ZetMapArea(0x8800, 0x8bff, 0, GalZ80Ram1 + 0x400);
	ZetMapArea(0x8800, 0x8bff, 1, GalZ80Ram1 + 0x400);
	ZetMapArea(0x8800, 0x8bff, 2, GalZ80Ram1 + 0x400);
	ZetMapArea(0x8c00, 0x8fff, 0, GalZ80Ram1 + 0x400);
	ZetMapArea(0x8c00, 0x8fff, 1, GalZ80Ram1 + 0x400);
	ZetMapArea(0x8c00, 0x8fff, 2, GalZ80Ram1 + 0x400);
	ZetClose();
	
	nGalCyclesTotal[1] = (14318000 / 8) / 60;
}

// DAC handlers
INT32 KingballSyncDAC()
{
	return (INT32)(float)(nBurnSoundLen * (ZetTotalCycles() / ((nGalCyclesTotal[1] * 60.0000) / (nBurnFPS / 100.0000))));
}

INT32 SfxSyncDAC()
{
	return (INT32)(float)(nBurnSoundLen * (ZetTotalCycles() / ((nGalCyclesTotal[2] * 60.0000) / (nBurnFPS / 100.0000))));
}

// Galaxian samples
static void GalRenderShootSample(INT16 *pSoundBuf, INT32 nLength)
{
	double Addr = GalShootWavePos;
	double Step = (double)GalShootRate / nBurnSoundRate;
	
	for (INT32 i = 0; i < nLength; i += 2) {
		INT16 Sample = (INT16)(GalShootWave[(INT32)Addr] * SHOOT_VOLUME);
		Sample >>= 4;
		
		INT32 nLeftSample = 0, nRightSample = 0;
			
		if ((GalOutputDir & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
			nLeftSample += (INT32)(Sample * GalGain);
		}
		if ((GalOutputDir & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
			nRightSample += (INT32)(Sample * GalGain);
		}
			
		nLeftSample = BURN_SND_CLIP(nLeftSample);
		nRightSample = BURN_SND_CLIP(nRightSample);
			
		pSoundBuf[i + 0] = BURN_SND_CLIP(pSoundBuf[i + 0] + nLeftSample);
		pSoundBuf[i + 1] = BURN_SND_CLIP(pSoundBuf[i + 1] + nRightSample);
		
		Addr += Step;
	}
	
	GalShootWavePos = Addr;
	if (GalShootWavePos > GalShootLength) {
		GalShootWavePos = 0;
		GalShootEnable = 0;
	}
}

static void GalRenderNoiseSample(INT16 *pSoundBuf, INT32 nLength)
{
	double Addr = GalNoiseWavePos;
	double Step = (double)NOISE_RATE / nBurnSoundRate;
	
	for (INT32 i = 0; i < nLength; i += 2) {
		INT16 Sample = (INT16)(GalNoiseWave[(INT32)Addr] * ((double)GalNoiseVolume / 100));
		Sample /= 10;
		Sample = biquad_explode2.filter(biquad_explode.filter(Sample) + Sample);

		INT32 nLeftSample = 0, nRightSample = 0;
			
		if ((GalOutputDir & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
			nLeftSample += (INT32)(Sample * GalGain);
		}
		if ((GalOutputDir & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
			nRightSample += (INT32)(Sample * GalGain);
		}

		nLeftSample = BURN_SND_CLIP(nLeftSample);
		nRightSample = BURN_SND_CLIP(nRightSample);

		pSoundBuf[i + 0] = BURN_SND_CLIP(pSoundBuf[i + 0] + nLeftSample);
		pSoundBuf[i + 1] = BURN_SND_CLIP(pSoundBuf[i + 1] + nRightSample);

		Addr += Step;
	}

	GalNoiseWavePos = Addr;
	if (GalNoiseWavePos > NOISE_LENGTH) {
		GalNoiseWavePos = 0;
	}
}

static void GalRenderToneWave(INT16 *pSoundBuf, INT32 nLength)
{
	INT32 i,j;
	INT16 *w = GalToneWave[GalVol];

	if (GalPitch != 0xff) {
		for (i = 0; i < nLength; i += 2) {
			INT32 mix = 0;

			for (j = 0; j < STEPS; j++) {
				if (GalCountDown >= (256 * nBurnSoundRate / 96000)) {
					GalCounter = (GalCounter + 1) % TOOTHSAW_LENGTH;
					GalCountDown = GalPitch * nBurnSoundRate / 96000;
				}
				GalCountDown++;

				mix += w[GalCounter % TOOTHSAW_LENGTH];
			}
			
			INT16 Sample = mix / STEPS;
			Sample >>= 4;
			
			INT32 nLeftSample = 0, nRightSample = 0;
			
			if ((GalOutputDir & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
				nLeftSample += (INT32)(Sample * GalGain);
			}
			if ((GalOutputDir & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
				nRightSample += (INT32)(Sample * GalGain);
			}
			
			nLeftSample = BURN_SND_CLIP(nLeftSample);
			nRightSample = BURN_SND_CLIP(nRightSample);
			
			pSoundBuf[i + 0] = nLeftSample;
			pSoundBuf[i + 1] = nRightSample;
		}
	}
}

static void GalRenderLfoWaveSample(INT32 nLfoWave, INT16 *pSoundBuf, INT32 nLength)
{
	INT32 nshHZ = 0;

	switch (nLfoWave) {
		case 0: nshHZ = 470; break;
		case 1: nshHZ = 330; break;
		case 2: nshHZ = 220; break;
	}
	double Addr = GalLfoWavePos[nLfoWave];
	double Step = (double)(sizeof(GalBackgroundWave) * GalLfoFreq * (100 + 2 * nshHZ) / (100 + 2 * 470)) / nBurnSoundRate;
	
	for (INT32 i = 0; i < nLength; i += 2) {
		INT16 Sample = (INT16)(GalBackgroundWave[(INT32)Addr] * (GalLfoVolume[nLfoWave] ? LFO_VOLUME : 0));
		Sample >>= 4;
		
		INT32 nLeftSample = 0, nRightSample = 0;
			
		if ((GalOutputDir & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
			nLeftSample += (INT32)(Sample * GalGain);
		}
		if ((GalOutputDir & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
			nRightSample += (INT32)(Sample * GalGain);
		}
			
		nLeftSample = BURN_SND_CLIP(nLeftSample);
		nRightSample = BURN_SND_CLIP(nRightSample);
			
		pSoundBuf[i + 0] = BURN_SND_CLIP(pSoundBuf[i + 0] + nLeftSample);
		pSoundBuf[i + 1] = BURN_SND_CLIP(pSoundBuf[i + 1] + nRightSample);
		
		Addr += Step;
	}
	
	GalLfoWavePos[nLfoWave] = Addr;
	if (GalLfoWavePos[nLfoWave] > 32) {
		GalLfoWavePos[nLfoWave] = 0;
	}
}

void GalRenderSoundSamples(INT16 *pSoundBuf, INT32 nLength)
{
	memset(pSoundBuf, 0, nLength * 2 * sizeof(INT16));
	
	GalRenderToneWave(pSoundBuf, nLength);
	GalRenderNoiseSample(pSoundBuf, nLength);
	if (GalShootEnable) GalRenderShootSample(pSoundBuf, nLength);
	GalRenderLfoWaveSample(0, pSoundBuf, nLength);
	GalRenderLfoWaveSample(1, pSoundBuf, nLength);
	GalRenderLfoWaveSample(2, pSoundBuf, nLength);
}

void GalaxianSoundWrite(UINT32 Offset, UINT8 d)
{
	d &= 0x01;

	switch (Offset & 0x07) {
		case 0x00:
		case 0x01:
		case 0x02: {
			GalLfoVolume[Offset] = d;
			return;
		}
		
		case 0x03: {
			if ((d & 1) && GalNoiseHold) {
				GalNoiseHold = 20; // already noise-ing, just extend the hold
				GalNoiseVolume = 100;
				return;
			}
			GalNoiseEnable = d & 1;
			if (GalNoiseEnable) {
				GalNoiseHold = 20;
				GalNoiseVolume = 100;
				GalNoiseWavePos = 0;
			}
			return;
		}
		
		case 0x04: {
			return;
		}
		
		case 0x05: {
			if (d & 1 && !(GalLastPort2 & 1) ) {
				GalShootEnable = 1;
				GalShootWavePos = 0;
			}
			GalLastPort2 = d;
			return;
		}
		
		case 0x06:
		case 0x07: {
			GalVol = (GalVol & ~(1 << (Offset & 0x01))) | ((d & 1) << (Offset & 0x01));
			return;
		}
	}
}

void GalaxianLfoFreqWrite(UINT32 Offset, UINT8 d)
{
	double r0, r1, rx = 100000.0;

	if ((d & 0x01) == GalLfoBit[Offset]) return;

	GalLfoBit[Offset] = d & 0x01;

	r0 = 1.0 / 330000;
	r1 = 1.0 / 1e12;

	if (GalLfoBit[0]) {
		r1 += 1.0 / 1000000;
	} else {
		r0 += 1.0 / 1000000;
	}

	if (GalLfoBit[1]) {
		r1 += 1.0 / 470000;
	} else {
		r0 += 1.0 / 470000;
	}

	if (GalLfoBit[2]) {
		r1 += 1.0 / 220000;
	} else {
		r0 += 1.0 / 220000;
	}

	if (GalLfoBit[3]) {
		r1 += 1.0 / 100000;
	} else {
		r0 += 1.0 / 100000;
	}

	r0 = 1.0 / r0;
	r1 = 1.0 / r1;

	rx = rx + 2000000.0 * r0 / (r0 + r1);

	GalLfoFreqFrameVar = (1000000 / (639 * rx / (MAXFREQ - MINFREQ))) * STEPS;
}

void GalaxianSoundUpdateTimers()
{
	if (GalNoiseHold) {
		GalNoiseHold--;
	}
	if ((GetCurrentFrame() % 0x3) == 0) {
		if (!GalNoiseHold && GalNoiseVolume > 0) {
			GalNoiseVolume -= (GalNoiseVolume / 10) + 1;
			if (GalNoiseVolume <= 0) {
				GalNoiseVolume = 0;
				GalNoiseEnable = 0;
			}
		}
	}

	if (GalLfoFreq > MINFREQ) {
		GalLfoFreq -= GalLfoFreqFrameVar;
	} else {
		GalLfoFreq = MAXFREQ;
	}
}
