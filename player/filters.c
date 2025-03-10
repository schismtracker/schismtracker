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

#include "player/sndfile.h"
#include "player/cmixer.h"

// LUT for 2 * damping factor
static const float resonance_table[128] = {
	1.0000000000000000f, 0.9786446094512940f, 0.9577452540397644f, 0.9372922182083130f,
	0.9172759056091309f, 0.8976871371269226f, 0.8785166740417481f, 0.8597555756568909f,
	0.8413951396942139f, 0.8234267830848694f, 0.8058421611785889f, 0.7886331081390381f,
	0.7717915177345276f, 0.7553095817565918f, 0.7391796708106995f, 0.7233941555023193f,
	0.7079457640647888f, 0.6928272843360901f, 0.6780316829681397f, 0.6635520458221436f,
	0.6493816375732422f, 0.6355138421058655f, 0.6219421625137329f, 0.6086603403091431f,
	0.5956621170043945f, 0.5829415321350098f, 0.5704925656318665f, 0.5583094954490662f,
	0.5463865399360657f, 0.5347182154655457f, 0.5232990980148315f, 0.5121238231658936f,
	0.5011872053146362f, 0.4904841780662537f, 0.4800096750259399f, 0.4697588682174683f,
	0.4597269892692566f, 0.4499093294143677f, 0.4403013288974762f, 0.4308985173702240f,
	0.4216965138912201f, 0.4126909971237183f, 0.4038778245449066f, 0.3952528536319733f,
	0.3868120610713959f, 0.3785515129566193f, 0.3704673945903778f, 0.3625559210777283f,
	0.3548133969306946f, 0.3472362160682678f, 0.3398208320140839f, 0.3325638175010681f,
	0.3254617750644684f, 0.3185114264488220f, 0.3117094635963440f, 0.3050527870655060f,
	0.2985382676124573f, 0.2921628654003143f, 0.2859236001968384f, 0.2798175811767578f,
	0.2738419771194458f, 0.2679939568042755f, 0.2622708380222321f, 0.2566699385643005f,
	0.2511886358261108f, 0.2458244115114212f, 0.2405747324228287f, 0.2354371547698975f,
	0.2304092943668366f, 0.2254888117313385f, 0.2206734120845795f, 0.2159608304500580f,
	0.2113489061594009f, 0.2068354636430740f, 0.2024184018373489f, 0.1980956792831421f,
	0.1938652694225311f, 0.1897251904010773f, 0.1856735348701477f, 0.1817083954811096f,
	0.1778279393911362f, 0.1740303486585617f, 0.1703138649463654f, 0.1666767448186874f,
	0.1631172895431519f, 0.1596338599920273f, 0.1562248021364212f, 0.1528885662555695f,
	0.1496235728263855f, 0.1464282870292664f, 0.1433012634515762f, 0.1402409970760346f,
	0.1372461020946503f, 0.1343151479959488f, 0.1314467936754227f, 0.1286396980285645f,
	0.1258925348520279f, 0.1232040524482727f, 0.1205729842185974f, 0.1179980933666229f,
	0.1154781952500343f, 0.1130121126770973f, 0.1105986908078194f, 0.1082368120551109f,
	0.1059253737330437f, 0.1036632955074310f, 0.1014495193958283f, 0.0992830246686935f,
	0.0971627980470657f, 0.0950878411531448f, 0.0930572077631950f, 0.0910699293017387f,
	0.0891250967979431f, 0.0872217938303947f, 0.0853591337800026f, 0.0835362523794174f,
	0.0817523002624512f, 0.0800064504146576f, 0.0782978758215904f, 0.0766257941722870f,
	0.0749894231557846f, 0.0733879879117012f, 0.0718207582831383f, 0.0702869966626167f,
	0.0687859877943993f, 0.0673170387744904f, 0.0658794566988945f, 0.0644725710153580f,
};


// Simple 2-poles resonant filter
//
// XXX freq WAS unused but is now mix_frequency!
//
#define FREQ_PARAM_MULT (128.0 / (24.0 * 256.0))
void setup_channel_filter(song_voice_t *chan, int32_t reset, int32_t flt_modifier, int32_t freq)
{
	int32_t cutoff = chan->cutoff;
	int32_t resonance = chan->resonance;
	float frequency, r, d, e, fg, fb0, fb1;

	cutoff = cutoff * (flt_modifier + 256) / 256;

	if (cutoff > 255)
		cutoff = 255;

	if (resonance > 255)
		resonance = 255;

	if (resonance == 0 && cutoff >= 254)
	{
		if (chan->flags & CHN_NEWNOTE)
		{
			// Z7F next to a note disables the filter, however in other cases this should not happen.
			// Test cases: filter-reset.it, filter-reset-carry.it, filter-reset-envelope.it, filter-nna.it, FilterResetPatDelay.it, FilterPortaSmpChange.it, FilterPortaSmpChange-InsMode.it
			chan->flags &= ~CHN_FILTER;
		}
		return;
	}
	chan->flags |= CHN_FILTER;

	// 2 ^ (i / 24 * 256)
	frequency = 110.0F * pow(2.0F, (float)cutoff * FREQ_PARAM_MULT + 0.25F);
	if (frequency > freq / 2.0F)
		frequency = freq / 2.0F;
	r = freq / (2.0F * M_PI * frequency);

	d = resonance_table[resonance] * r + resonance_table[resonance] - 1.0F;
	e = r * r;

	fg = 1.0F / (1.0F + d + e);
	fb0 = (d + e + e) / (1.0F + d + e);
	fb1 = -e / (1.0F + d + e);

	chan->filter_a0 = (int32_t)(fg * (1 << FILTERPRECISION));
	chan->filter_b0 = (int32_t)(fb0 * (1 << FILTERPRECISION));
	chan->filter_b1 = (int32_t)(fb1 * (1 << FILTERPRECISION));

	if (reset) {
		chan->filter_y[0][0] = chan->filter_y[0][1] = 0;
		chan->filter_y[1][0] = chan->filter_y[1][1] = 0;
	}
}

