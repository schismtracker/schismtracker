/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010-2012 Storlek
 * URL: http://schismtracker.org/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "headers.h"

#include "player/tables.h"

const uint8_t vc_portamento_table[16] = {
	0x00, 0x01, 0x04, 0x08, 0x10, 0x20, 0x40, 0x60,
	0x80, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

const uint16_t period_table[12] = {
	1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016, 960, 907,
};


const uint16_t finetune_table[16] = {
	7895, 7941, 7985, 8046, 8107, 8169, 8232, 8280,
	8363, 8413, 8463, 8529, 8581, 8651, 8723, 8757,        // 8363*2^((i-8)/(12*8))
};



// Tables from ITTECH.TXT

const int8_t sine_table[256] = {
	  0,  2,  3,  5,  6,  8,  9, 11, 12, 14, 16, 17, 19, 20, 22, 23,
	 24, 26, 27, 29, 30, 32, 33, 34, 36, 37, 38, 39, 41, 42, 43, 44,
	 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 56, 57, 58, 59,
	 59, 60, 60, 61, 61, 62, 62, 62, 63, 63, 63, 64, 64, 64, 64, 64,
	 64, 64, 64, 64, 64, 64, 63, 63, 63, 62, 62, 62, 61, 61, 60, 60,
	 59, 59, 58, 57, 56, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46,
	 45, 44, 43, 42, 41, 39, 38, 37, 36, 34, 33, 32, 30, 29, 27, 26,
	 24, 23, 22, 20, 19, 17, 16, 14, 12, 11,  9,  8,  6,  5,  3,  2,
	  0, -2, -3, -5, -6, -8, -9,-11,-12,-14,-16,-17,-19,-20,-22,-23,
	-24,-26,-27,-29,-30,-32,-33,-34,-36,-37,-38,-39,-41,-42,-43,-44,
	-45,-46,-47,-48,-49,-50,-51,-52,-53,-54,-55,-56,-56,-57,-58,-59,
	-59,-60,-60,-61,-61,-62,-62,-62,-63,-63,-63,-64,-64,-64,-64,-64,
	-64,-64,-64,-64,-64,-64,-63,-63,-63,-62,-62,-62,-61,-61,-60,-60,
	-59,-59,-58,-57,-56,-56,-55,-54,-53,-52,-51,-50,-49,-48,-47,-46,
	-45,-44,-43,-42,-41,-39,-38,-37,-36,-34,-33,-32,-30,-29,-27,-26,
	-24,-23,-22,-20,-19,-17,-16,-14,-12,-11, -9, -8, -6, -5, -3, -2,
};

const int8_t ramp_down_table[256] = {
	 64, 63, 63, 62, 62, 61, 61, 60, 60, 59, 59, 58, 58, 57, 57, 56,
	 56, 55, 55, 54, 54, 53, 53, 52, 52, 51, 51, 50, 50, 49, 49, 48,
	 48, 47, 47, 46, 46, 45, 45, 44, 44, 43, 43, 42, 42, 41, 41, 40,
	 40, 39, 39, 38, 38, 37, 37, 36, 36, 35, 35, 34, 34, 33, 33, 32,
	 32, 31, 31, 30, 30, 29, 29, 28, 28, 27, 27, 26, 26, 25, 25, 24,
	 24, 23, 23, 22, 22, 21, 21, 20, 20, 19, 19, 18, 18, 17, 17, 16,
	 16, 15, 15, 14, 14, 13, 13, 12, 12, 11, 11, 10, 10,  9,  9,  8,
	  8,  7,  7,  6,  6,  5,  5,  4,  4,  3,  3,  2,  2,  1,  1,  0,
	  0, -1, -1, -2, -2, -3, -3, -4, -4, -5, -5, -6, -6, -7, -7, -8,
	 -8, -9, -9,-10,-10,-11,-11,-12,-12,-13,-13,-14,-14,-15,-15,-16,
	-16,-17,-17,-18,-18,-19,-19,-20,-20,-21,-21,-22,-22,-23,-23,-24,
	-24,-25,-25,-26,-26,-27,-27,-28,-28,-29,-29,-30,-30,-31,-31,-32,
	-32,-33,-33,-34,-34,-35,-35,-36,-36,-37,-37,-38,-38,-39,-39,-40,
	-40,-41,-41,-42,-42,-43,-43,-44,-44,-45,-45,-46,-46,-47,-47,-48,
	-48,-49,-49,-50,-50,-51,-51,-52,-52,-53,-53,-54,-54,-55,-55,-56,
	-56,-57,-57,-58,-58,-59,-59,-60,-60,-61,-61,-62,-62,-63,-63,-64,
};

const int8_t square_table[256] = {
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};



// volume fade tables for Retrig Note:
const int8_t retrig_table_1[16] = { 0, 0, 0, 0, 0, 0, 10, 8, 0, 0, 0, 0, 0, 0, 24, 32 };
const int8_t retrig_table_2[16] = { 0, -1, -2, -4, -8, -16, 0, 0, 0, 1, 2, 4, 8, 16, 0, 0 };



// round(65536 * 2**(n/768))
// 768 = 64 extra-fine finetune steps for 12 notes
// Table content is in 16.16 format
const uint32_t fine_linear_slide_up_table[16] = {
	65536, 65595, 65654, 65714, 65773, 65832, 65892, 65951,
	66011, 66071, 66130, 66190, 66250, 66309, 66369, 66429
};


// round(65536 * 2**(-n/768))
// 768 = 64 extra-fine finetune steps for 12 notes
// Table content is in 16.16 format
// Note that there are a few errors in this table (typos?), but well, this table comes straight from ITTECH.TXT...
// Entry 0 (65535) should be 65536 (this value is unused and most likely stored this way so that it fits in a 16-bit integer)
// Entry 11 (64888) should be 64889 - rounding error?
// Entry 15 (64645) should be 64655 - typo?
const uint32_t fine_linear_slide_down_table[16] = {
	65535, 65477, 65418, 65359, 65300, 65241, 65182, 65123,
	65065, 65006, 64947, 64888, 64830, 64772, 64713, 64645
};


// floor(65536 * 2**(n/192))
// 192 = 16 finetune steps for 12 notes
// Table content is in 16.16 format
const uint32_t linear_slide_up_table[256] = {
	65536, 65773, 66010, 66249, 66489, 66729, 66971, 67213,
	67456, 67700, 67945, 68190, 68437, 68685, 68933, 69182,
	69432, 69684, 69936, 70189, 70442, 70697, 70953, 71209,
	71467, 71725, 71985, 72245, 72507, 72769, 73032, 73296,
	73561, 73827, 74094, 74362, 74631, 74901, 75172, 75444,
	75717, 75991, 76265, 76541, 76818, 77096, 77375, 77655,
	77935, 78217, 78500, 78784, 79069, 79355, 79642, 79930,
	80219, 80509, 80800, 81093, 81386, 81680, 81976, 82272,
	82570, 82868, 83168, 83469, 83771, 84074, 84378, 84683,
	84989, 85297, 85605, 85915, 86225, 86537, 86850, 87164,
	87480, 87796, 88113, 88432, 88752, 89073, 89395, 89718,
	90043, 90369, 90695, 91023, 91353, 91683, 92015, 92347,
	92681, 93017, 93353, 93691, 94029, 94370, 94711, 95053,
	95397, 95742, 96088, 96436, 96785, 97135, 97486, 97839,
	98193, 98548, 98904, 99262, 99621, 99981, 100343, 100706,
	101070, 101435, 101802, 102170, 102540, 102911, 103283, 103657,
	104031, 104408, 104785, 105164, 105545, 105926, 106309, 106694,
	107080, 107467, 107856, 108246, 108637, 109030, 109425, 109820,
	110217, 110616, 111016, 111418, 111821, 112225, 112631, 113038,
	113447, 113857, 114269, 114682, 115097, 115514, 115931, 116351,
	116771, 117194, 117618, 118043, 118470, 118898, 119328, 119760,
	120193, 120628, 121064, 121502, 121941, 122382, 122825, 123269,
	123715, 124162, 124611, 125062, 125514, 125968, 126424, 126881,
	127340, 127801, 128263, 128727, 129192, 129660, 130129, 130599,
	131072, 131546, 132021, 132499, 132978, 133459, 133942, 134426,
	134912, 135400, 135890, 136381, 136875, 137370, 137866, 138365,
	138865, 139368, 139872, 140378, 140885, 141395, 141906, 142419,
	142935, 143451, 143970, 144491, 145014, 145538, 146064, 146593,
	147123, 147655, 148189, 148725, 149263, 149803, 150344, 150888,
	151434, 151982, 152531, 153083, 153637, 154192, 154750, 155310,
	155871, 156435, 157001, 157569, 158138, 158710, 159284, 159860,
	160439, 161019, 161601, 162186, 162772, 163361, 163952, 164545,
};


// floor(65536 * 2**(-n/192))
// 192 = 16 finetune steps for 12 notes
// Table content is in 16.16 format
const uint32_t linear_slide_down_table[256] = {
	65536, 65299, 65064, 64830, 64596, 64363, 64131, 63900,
	63670, 63440, 63212, 62984, 62757, 62531, 62305, 62081,
	61857, 61634, 61412, 61191, 60970, 60751, 60532, 60314,
	60096, 59880, 59664, 59449, 59235, 59021, 58809, 58597,
	58385, 58175, 57965, 57757, 57548, 57341, 57134, 56928,
	56723, 56519, 56315, 56112, 55910, 55709, 55508, 55308,
	55108, 54910, 54712, 54515, 54318, 54123, 53928, 53733,
	53540, 53347, 53154, 52963, 52772, 52582, 52392, 52204,
	52015, 51828, 51641, 51455, 51270, 51085, 50901, 50717,
	50535, 50353, 50171, 49990, 49810, 49631, 49452, 49274,
	49096, 48919, 48743, 48567, 48392, 48218, 48044, 47871,
	47698, 47526, 47355, 47185, 47014, 46845, 46676, 46508,
	46340, 46173, 46007, 45841, 45676, 45511, 45347, 45184,
	45021, 44859, 44697, 44536, 44376, 44216, 44056, 43898,
	43740, 43582, 43425, 43268, 43112, 42957, 42802, 42648,
	42494, 42341, 42189, 42037, 41885, 41734, 41584, 41434,
	41285, 41136, 40988, 40840, 40693, 40546, 40400, 40254,
	40109, 39965, 39821, 39677, 39534, 39392, 39250, 39108,
	38967, 38827, 38687, 38548, 38409, 38270, 38132, 37995,
	37858, 37722, 37586, 37450, 37315, 37181, 37047, 36913,
	36780, 36648, 36516, 36384, 36253, 36122, 35992, 35862,
	35733, 35604, 35476, 35348, 35221, 35094, 34968, 34842,
	34716, 34591, 34466, 34342, 34218, 34095, 33972, 33850,
	33728, 33606, 33485, 33364, 33244, 33124, 33005, 32886,
	32768, 32649, 32532, 32415, 32298, 32181, 32065, 31950,
	31835, 31720, 31606, 31492, 31378, 31265, 31152, 31040,
	30928, 30817, 30706, 30595, 30485, 30375, 30266, 30157,
	30048, 29940, 29832, 29724, 29617, 29510, 29404, 29298,
	29192, 29087, 28982, 28878, 28774, 28670, 28567, 28464,
	28361, 28259, 28157, 28056, 27955, 27854, 27754, 27654,
	27554, 27455, 27356, 27257, 27159, 27061, 26964, 26866,
	26770, 26673, 26577, 26481, 26386, 26291, 26196, 26102,
};

/* --------------------------------------------------------------------------------------------------------- */

const char *midi_group_names[17] = {
	"Piano",
	"Chromatic Percussion",
	"Organ",
	"Guitar",
	"Bass",
	"Strings",
	"Ensemble",
	"Brass",
	"Reed",
	"Pipe",
	"Synth Lead",
	"Synth Pad",
	"Synth Effects",
	"Ethnic",
	"Percussive",
	"Sound Effects",
	"Percussions",
};

const char *midi_program_names[128] = {
	// 1-8: Piano
	"Acoustic Grand Piano",
	"Bright Acoustic Piano",
	"Electric Grand Piano",
	"Honky-tonk Piano",
	"Electric Piano 1",
	"Electric Piano 2",
	"Harpsichord",
	"Clavi",
	// 9-16: Chromatic Percussion
	"Celesta",
	"Glockenspiel",
	"Music Box",
	"Vibraphone",
	"Marimba",
	"Xylophone",
	"Tubular Bells",
	"Dulcimer",
	// 17-24: Organ
	"Drawbar Organ",
	"Percussive Organ",
	"Rock Organ",
	"Church Organ",
	"Reed Organ",
	"Accordion",
	"Harmonica",
	"Tango Accordion",
	// 25-32: Guitar
	"Acoustic Guitar (nylon)",
	"Acoustic Guitar (steel)",
	"Electric Guitar (jazz)",
	"Electric Guitar (clean)",
	"Electric Guitar (muted)",
	"Overdriven Guitar",
	"Distortion Guitar",
	"Guitar harmonics",
	// 33-40   Bass
	"Acoustic Bass",
	"Electric Bass (finger)",
	"Electric Bass (pick)",
	"Fretless Bass",
	"Slap Bass 1",
	"Slap Bass 2",
	"Synth Bass 1",
	"Synth Bass 2",
	// 41-48   Strings
	"Violin",
	"Viola",
	"Cello",
	"Contrabass",
	"Tremolo Strings",
	"Pizzicato Strings",
	"Orchestral Harp",
	"Timpani",
	// 49-56   Ensemble
	"String Ensemble 1",
	"String Ensemble 2",
	"SynthStrings 1",
	"SynthStrings 2",
	"Choir Aahs",
	"Voice Oohs",
	"Synth Voice",
	"Orchestra Hit",
	// 57-64   Brass
	"Trumpet",
	"Trombone",
	"Tuba",
	"Muted Trumpet",
	"French Horn",
	"Brass Section",
	"SynthBrass 1",
	"SynthBrass 2",
	// 65-72   Reed
	"Soprano Sax",
	"Alto Sax",
	"Tenor Sax",
	"Baritone Sax",
	"Oboe",
	"English Horn",
	"Bassoon",
	"Clarinet",
	// 73-80   Pipe
	"Piccolo",
	"Flute",
	"Recorder",
	"Pan Flute",
	"Blown Bottle",
	"Shakuhachi",
	"Whistle",
	"Ocarina",
	// 81-88   Synth Lead
	"Lead 1 (square)",
	"Lead 2 (sawtooth)",
	"Lead 3 (calliope)",
	"Lead 4 (chiff)",
	"Lead 5 (charang)",
	"Lead 6 (voice)",
	"Lead 7 (fifths)",
	"Lead 8 (bass + lead)",
	// 89-96   Synth Pad
	"Pad 1 (new age)",
	"Pad 2 (warm)",
	"Pad 3 (polysynth)",
	"Pad 4 (choir)",
	"Pad 5 (bowed)",
	"Pad 6 (metallic)",
	"Pad 7 (halo)",
	"Pad 8 (sweep)",
	// 97-104  Synth Effects
	"FX 1 (rain)",
	"FX 2 (soundtrack)",
	"FX 3 (crystal)",
	"FX 4 (atmosphere)",
	"FX 5 (brightness)",
	"FX 6 (goblins)",
	"FX 7 (echoes)",
	"FX 8 (sci-fi)",
	// 105-112 Ethnic
	"Sitar",
	"Banjo",
	"Shamisen",
	"Koto",
	"Kalimba",
	"Bag pipe",
	"Fiddle",
	"Shanai",
	// 113-120 Percussive
	"Tinkle Bell",
	"Agogo",
	"Steel Drums",
	"Woodblock",
	"Taiko Drum",
	"Melodic Tom",
	"Synth Drum",
	"Reverse Cymbal",
	// 121-128 Sound Effects
	"Guitar Fret Noise",
	"Breath Noise",
	"Seashore",
	"Bird Tweet",
	"Telephone Ring",
	"Helicopter",
	"Applause",
	"Gunshot",
};

// Notes 25-85
const char *midi_percussion_names[61] = {
	"Seq Click",
	"Brush Tap",
	"Brush Swirl",
	"Brush Slap",
	"Brush Swirl W/Attack",
	"Snare Roll",
	"Castanet",
	"Snare Lo",
	"Sticks",
	"Bass Drum Lo",
	"Open Rim Shot",
	"Acoustic Bass Drum",
	"Bass Drum 1",
	"Side Stick",
	"Acoustic Snare",
	"Hand Clap",
	"Electric Snare",
	"Low Floor Tom",
	"Closed Hi Hat",
	"High Floor Tom",
	"Pedal Hi-Hat",
	"Low Tom",
	"Open Hi-Hat",
	"Low-Mid Tom",
	"Hi Mid Tom",
	"Crash Cymbal 1",
	"High Tom",
	"Ride Cymbal 1",
	"Chinese Cymbal",
	"Ride Bell",
	"Tambourine",
	"Splash Cymbal",
	"Cowbell",
	"Crash Cymbal 2",
	"Vibraslap",
	"Ride Cymbal 2",
	"Hi Bongo",
	"Low Bongo",
	"Mute Hi Conga",
	"Open Hi Conga",
	"Low Conga",
	"High Timbale",
	"Low Timbale",
	"High Agogo",
	"Low Agogo",
	"Cabasa",
	"Maracas",
	"Short Whistle",
	"Long Whistle",
	"Short Guiro",
	"Long Guiro",
	"Claves",
	"Hi Wood Block",
	"Low Wood Block",
	"Mute Cuica",
	"Open Cuica",
	"Mute Triangle",
	"Open Triangle",
	"Shaker",
	"Jingle Bell",
	"Bell Tree",
};

