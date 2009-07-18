/*
 * This source code is public domain.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
*/

///////////////////////////////////////////////////////
// DMF DELUSION DIGITAL MUSIC FILEFORMAT (X-Tracker) //
///////////////////////////////////////////////////////
#include "sndfile.h"

//#define DMFLOG

//#pragma warning(disable:4244)

#pragma pack(1)

typedef struct DMFHEADER
{
	uint32_t id;				// "DDMF" = 0x464d4444
	uint8_t version;			// 4
	int8_t trackername[8];	// "XTRACKER"
	int8_t songname[30];
	int8_t composer[20];
	uint8_t date[3];
} DMFHEADER;

typedef struct DMFINFO
{
	uint32_t id;			// "INFO"
	uint32_t infosize;
} DMFINFO;

typedef struct DMFSEQU
{
	uint32_t id;			// "SEQU"
	uint32_t seqsize;
	uint16_t loopstart;
	uint16_t loopend;
	uint16_t sequ[2];
} DMFSEQU;

typedef struct DMFPATT
{
	uint32_t id;			// "PATT"
	uint32_t patsize;
	uint16_t numpat;		// 1-1024
	uint8_t tracks;
	uint8_t firstpatinfo;
} DMFPATT;

typedef struct DMFTRACK
{
	uint8_t tracks;
	uint8_t beat;		// [hi|lo] -> hi=ticks per beat, lo=beats per measure
	uint16_t ticks;		// max 512
	uint32_t jmpsize;
} DMFTRACK;

typedef struct DMFSMPI
{
	uint32_t id;
	uint32_t size;
	uint8_t samples;
} DMFSMPI;

typedef struct DMFSAMPLE
{
	uint32_t len;
	uint32_t loopstart;
	uint32_t loopend;
	uint16_t c3speed;
	uint8_t volume;
	uint8_t flags;
} DMFSAMPLE;

#pragma pack()


#ifdef DMFLOG
extern void Log(const char * s, ...);
#endif


bool CSoundFile::ReadDMF(const uint8_t *lpStream, uint32_t dwMemLength)
//---------------------------------------------------------------
{
	DMFHEADER *pfh = (DMFHEADER *)lpStream;
	DMFINFO *psi;
	DMFSEQU *sequ;
	uint32_t dwMemPos;
	uint8_t infobyte[32];
	uint8_t smplflags[MAX_SAMPLES];

	if ((!lpStream) || (dwMemLength < 1024)) return false;
	if ((pfh->id != 0x464d4444) || (!pfh->version) || (pfh->version & 0xF0)) return false;
	dwMemPos = 66;
	memcpy(song_title, pfh->songname, 30);
	song_title[30] = 0;
	m_nType = MOD_TYPE_DMF;
	m_nChannels = 0;
#ifdef DMFLOG
	Log("DMF version %d: \"%s\": %d bytes (0x%04X)\n", pfh->version, song_title, dwMemLength, dwMemLength);
#endif
	while (dwMemPos + 7 < dwMemLength)
	{
		uint32_t id = *((uint32_t *)(lpStream+dwMemPos));

		switch(id)
		{
		// "INFO"
		case 0x4f464e49:
		// "CMSG"
		case 0x47534d43:
			psi = (DMFINFO *)(lpStream+dwMemPos);
			if (id == 0x47534d43) dwMemPos++;
			if ((psi->infosize > dwMemLength) || (psi->infosize + dwMemPos + 8 > dwMemLength)) goto dmfexit;
			if ((psi->infosize >= 8) && (!m_lpszSongComments))
			{
				uint32_t len = MIN(psi->infosize - 1, MAX_MESSAGE);
				for (uint32_t i=0; i < len; i++) {
					int8_t c = lpStream[dwMemPos+8+i];
					if ((i % 40) == 39)
						m_lpszSongComments[i] = '\n';
					else
						m_lpszSongComments[i] = MAX(c, ' ');
				}
				m_lpszSongComments[len] = 0;
			}
			dwMemPos += psi->infosize + 8 - 1;
			break;

		// "SEQU"
		case 0x55514553:
			sequ = (DMFSEQU *)(lpStream+dwMemPos);
			if ((sequ->seqsize >= dwMemLength) || (dwMemPos + sequ->seqsize + 12 > dwMemLength)) goto dmfexit;
			{
				uint32_t nseq = sequ->seqsize >> 1;
				if (nseq >= MAX_ORDERS-1) nseq = MAX_ORDERS-1;
				//if (sequ->loopstart < nseq) m_nRestartPos = sequ->loopstart;
				for (uint32_t i=0; i<nseq; i++) Orderlist[i] = (uint8_t)sequ->sequ[i];
			}
			dwMemPos += sequ->seqsize + 8;
			break;

		// "PATT"
		case 0x54544150:
			if (!m_nChannels)
			{
				DMFPATT *patt = (DMFPATT *)(lpStream+dwMemPos);
				uint32_t numpat;
				uint32_t dwPos = dwMemPos + 11;
				if ((patt->patsize >= dwMemLength) || (dwMemPos + patt->patsize + 8 > dwMemLength)) goto dmfexit;
				numpat = patt->numpat;
				if (numpat > MAX_PATTERNS) numpat = MAX_PATTERNS;
				m_nChannels = patt->tracks;
				if (m_nChannels < patt->firstpatinfo) m_nChannels = patt->firstpatinfo;
				if (m_nChannels > 32) m_nChannels = 32;
				if (m_nChannels < 4) m_nChannels = 4;
				for (uint32_t npat=0; npat<numpat; npat++)
				{
					DMFTRACK *pt = (DMFTRACK *)(lpStream+dwPos);
				#ifdef DMFLOG
					Log("Pattern #%d: %d tracks, %d rows\n", npat, pt->tracks, pt->ticks);
				#endif
					uint32_t tracks = pt->tracks;
					if (tracks > 32) tracks = 32;
					uint32_t ticks = pt->ticks;
					if (ticks > 256) ticks = 256;
					if (ticks < 16) ticks = 16;
					dwPos += 8;
					if ((pt->jmpsize >= dwMemLength) || (dwPos + pt->jmpsize + 4 >= dwMemLength)) break;
					PatternSize[npat] = (uint16_t)ticks;
					PatternAllocSize[npat] = (uint16_t)ticks;
					MODCOMMAND *m = csf_allocate_pattern(PatternSize[npat], m_nChannels);
					if (!m) goto dmfexit;
					Patterns[npat] = m;
					uint32_t d = dwPos;
					dwPos += pt->jmpsize;
					uint32_t ttype = 1;
					uint32_t tempo = 125;
					uint32_t glbinfobyte = 0;
					uint32_t pbeat = (pt->beat & 0xf0) ? pt->beat>>4 : 8;
					bool tempochange = (pt->beat & 0xf0) ? true : false;
					memset(infobyte, 0, sizeof(infobyte));
					for (uint32_t row=0; row<ticks; row++)
					{
						MODCOMMAND *p = &m[row*m_nChannels];
						// Parse track global effects
						if (!glbinfobyte)
						{
							uint8_t info = lpStream[d++];
							uint8_t infoval = 0;
							if ((info & 0x80) && (d < dwPos)) glbinfobyte = lpStream[d++];
							info &= 0x7f;
							if ((info) && (d < dwPos)) infoval = lpStream[d++];
							switch(info)
							{
							case 1:	ttype = 0; tempo = infoval; tempochange = true; break;
							case 2: ttype = 1; tempo = infoval; tempochange = true; break;
							case 3: pbeat = infoval>>4; tempochange = ttype; break;
							#ifdef DMFLOG
							default: if (info) Log("GLB: %02X.%02X\n", info, infoval);
							#endif
							}
						} else
						{
							glbinfobyte--;
						}
						// Parse channels
						for (uint32_t i=0; i<tracks; i++) if (!infobyte[i])
						{
							MODCOMMAND cmd = {0,0,0,0,0,0};
							uint8_t info = lpStream[d++];
							if (info & 0x80) infobyte[i] = lpStream[d++];
							// Instrument
							if (info & 0x40)
							{
								cmd.instr = lpStream[d++];
							}
							// Note
							if (info & 0x20)
							{
								cmd.note = lpStream[d++];
								if ((cmd.note) && (cmd.note < 0xfe)) cmd.note &= 0x7f;
								if ((cmd.note) && (cmd.note < 128)) cmd.note += 24;
							}
							// Volume
							if (info & 0x10)
							{
								cmd.volcmd = VOLCMD_VOLUME;
								cmd.vol = (lpStream[d++]+3)>>2;
							}
							// Effect 1
							if (info & 0x08)
							{
								uint8_t efx = lpStream[d++];
								uint8_t eval = lpStream[d++];
								switch(efx)
								{
								// 1: Key Off
								case 1: if (!cmd.note) cmd.note = 0xFE; break;
								// 2: Set Loop
								// 4: Sample Delay
								case 4: if (eval&0xe0) { cmd.command = CMD_S3MCMDEX; cmd.param = (eval>>5)|0xD0; } break;
								// 5: Retrig
								case 5: if (eval&0xe0) { cmd.command = CMD_RETRIG; cmd.param = (eval>>5); } break;
								// 6: Offset
								case 6: cmd.command = CMD_OFFSET; cmd.param = eval; break;
								#ifdef DMFLOG
								default: Log("FX1: %02X.%02X\n", efx, eval);
								#endif
								}
							}
							// Effect 2
							if (info & 0x04)
							{
								uint8_t efx = lpStream[d++];
								uint8_t eval = lpStream[d++];
								switch(efx)
								{
								// 1: Finetune
								case 1: if (eval&0xf0) { cmd.command = CMD_S3MCMDEX; cmd.param = (eval>>4)|0x20; } break;
								// 2: Note Delay
								case 2: if (eval&0xe0) { cmd.command = CMD_S3MCMDEX; cmd.param = (eval>>5)|0xD0; } break;
								// 3: Arpeggio
								case 3: if (eval) { cmd.command = CMD_ARPEGGIO; cmd.param = eval; } break;
								// 4: Portamento Up
								case 4: cmd.command = CMD_PORTAMENTOUP; cmd.param = (eval >= 0xe0) ? 0xdf : eval; break;
								// 5: Portamento Down
								case 5: cmd.command = CMD_PORTAMENTODOWN; cmd.param = (eval >= 0xe0) ? 0xdf : eval; break;
								// 6: Tone Portamento
								case 6: cmd.command = CMD_TONEPORTAMENTO; cmd.param = eval; break;
								// 8: Vibrato
								case 8: cmd.command = CMD_VIBRATO; cmd.param = eval; break;
								// 12: Note cut
								case 12: if (eval & 0xe0) { cmd.command = CMD_S3MCMDEX; cmd.param = (eval>>5)|0xc0; }
										else if (!cmd.note) { cmd.note = 0xfe; } break;
								#ifdef DMFLOG
								default: Log("FX2: %02X.%02X\n", efx, eval);
								#endif
								}
							}
							// Effect 3
							if (info & 0x02)
							{
								uint8_t efx = lpStream[d++];
								uint8_t eval = lpStream[d++];
								switch(efx)
								{
								// 1: Vol Slide Up
								case 1: if (eval == 0xff) break;
										eval = (eval+3)>>2; if (eval > 0x0f) eval = 0x0f;
										cmd.command = CMD_VOLUMESLIDE; cmd.param = eval<<4; break;
								// 2: Vol Slide Down
								case 2:	if (eval == 0xff) break;
										eval = (eval+3)>>2; if (eval > 0x0f) eval = 0x0f;
										cmd.command = CMD_VOLUMESLIDE; cmd.param = eval; break;
								// 7: Set Pan
								case 7: if (!cmd.volcmd) { cmd.volcmd = VOLCMD_PANNING; cmd.vol = (eval+3)>>2; }
										else { cmd.command = CMD_PANNING8; cmd.param = eval; } break;
								// 8: Pan Slide Left
								case 8: eval = (eval+3)>>2; if (eval > 0x0f) eval = 0x0f;
										cmd.command = CMD_PANNINGSLIDE; cmd.param = eval<<4; break;
								// 9: Pan Slide Right
								case 9: eval = (eval+3)>>2; if (eval > 0x0f) eval = 0x0f;
										cmd.command = CMD_PANNINGSLIDE; cmd.param = eval; break;
								#ifdef DMFLOG
								default: Log("FX3: %02X.%02X\n", efx, eval);
								#endif

								}
							}
							// Store effect
							if (i < m_nChannels) p[i] = cmd;
							if (d > dwPos)
							{
							#ifdef DMFLOG
								Log("Unexpected EOP: row=%d\n", row);
							#endif
								break;
							}
						} else
						{
							infobyte[i]--;
						}

						// Find free channel for tempo change
						if (tempochange)
						{
							tempochange = false;
							uint32_t speed=6, modtempo=tempo;
							uint32_t rpm = ((ttype) && (pbeat)) ? tempo*pbeat : (tempo+1)*15;
							for (speed=30; speed>1; speed--)
							{
								modtempo = rpm*speed/24;
								if (modtempo <= 200) break;
								if ((speed < 6) && (modtempo < 256)) break;
							}
						#ifdef DMFLOG
							Log("Tempo change: ttype=%d pbeat=%d tempo=%3d -> speed=%d tempo=%d\n",
								ttype, pbeat, tempo, speed, modtempo);
						#endif
							for (uint32_t ich=0; ich<m_nChannels; ich++) if (!p[ich].command)
							{
								if (speed)
								{
									p[ich].command = CMD_SPEED;
									p[ich].param = (uint8_t)speed;
									speed = 0;
								} else
								if ((modtempo >= 32) && (modtempo < 256))
								{
									p[ich].command = CMD_TEMPO;
									p[ich].param = (uint8_t)modtempo;
									modtempo = 0;
								} else
								{
									break;
								}
							}
						}
						if (d >= dwPos) break;
					}
				#ifdef DMFLOG
					Log(" %d/%d bytes remaining\n", dwPos-d, pt->jmpsize);
				#endif
					if (dwPos + 8 >= dwMemLength) break;
				}
				dwMemPos += patt->patsize + 8;
			}
			break;

		// "SMPI": Sample Info
		case 0x49504d53:
			{
				DMFSMPI *pds = (DMFSMPI *)(lpStream+dwMemPos);
				if (pds->size <= dwMemLength - dwMemPos)
				{
					uint32_t dwPos = dwMemPos + 9;
					m_nSamples = pds->samples;
					if (m_nSamples >= MAX_SAMPLES) m_nSamples = MAX_SAMPLES-1;
					for (uint32_t iSmp=1; iSmp<=m_nSamples; iSmp++)
					{
						uint32_t namelen = lpStream[dwPos];
						smplflags[iSmp] = 0;
						if (dwPos+namelen+1+sizeof(DMFSAMPLE) > dwMemPos+pds->size+8) break;
						if (namelen)
						{
							uint32_t rlen = (namelen < 32) ? namelen : 31;
							memcpy(Samples[iSmp].name, lpStream+dwPos+1, rlen);
							Samples[iSmp].name[rlen] = 0;
						}
						dwPos += namelen + 1;
						DMFSAMPLE *psh = (DMFSAMPLE *)(lpStream+dwPos);
						SONGSAMPLE *psmp = &Samples[iSmp];
						psmp->nLength = psh->len;
						psmp->nLoopStart = psh->loopstart;
						psmp->nLoopEnd = psh->loopend;
						psmp->nC5Speed = psh->c3speed;
						psmp->nGlobalVol = 64;
						psmp->nVolume = (psh->volume) ? ((uint16_t)psh->volume)+1 : (uint16_t)256;
						psmp->uFlags = (psh->flags & 2) ? CHN_16BIT : 0;
						if (psmp->uFlags & CHN_16BIT) psmp->nLength >>= 1;
						if (psh->flags & 1) psmp->uFlags |= CHN_LOOP;
						smplflags[iSmp] = psh->flags;
						dwPos += (pfh->version < 8) ? 22 : 30;
					#ifdef DMFLOG
						Log("SMPI %d/%d: len=%d flags=0x%02X\n", iSmp, m_nSamples, psmp->nLength, psh->flags);
					#endif
					}
				}
				dwMemPos += pds->size + 8;
			}
			break;

		// "SMPD": Sample Data
		case 0x44504d53:
			{
				uint32_t dwPos = dwMemPos + 8;
				uint32_t ismpd = 0;
				for (uint32_t iSmp=1; iSmp<=m_nSamples; iSmp++)
				{
					ismpd++;
					uint32_t pksize;
					if (dwPos + 4 >= dwMemLength)
					{
					#ifdef DMFLOG
						Log("Unexpected EOF at sample %d/%d! (pos=%d)\n", iSmp, m_nSamples, dwPos);
					#endif
						break;
					}
					pksize = *((uint32_t *)(lpStream+dwPos));
				#ifdef DMFLOG
					Log("sample %d: pos=0x%X pksize=%d ", iSmp, dwPos, pksize);
					Log("len=%d flags=0x%X [%08X]\n", Samples[iSmp].nLength, smplflags[ismpd], *((uint32_t *)(lpStream+dwPos+4)));
				#endif
					dwPos += 4;
					if (pksize > dwMemLength - dwPos)
					{
					#ifdef DMFLOG
						Log("WARNING: pksize=%d, but only %d bytes left\n", pksize, dwMemLength-dwPos);
					#endif
						pksize = dwMemLength - dwPos;
					}
					if ((pksize) && (iSmp <= m_nSamples))
					{
						uint32_t flags = (Samples[iSmp].uFlags & CHN_16BIT) ? RS_PCM16S : RS_PCM8S;
						if (smplflags[ismpd] & 4) flags = (Samples[iSmp].uFlags & CHN_16BIT) ? RS_DMF16 : RS_DMF8;
						csf_read_sample(&Samples[iSmp], flags, (const char *)(lpStream+dwPos), pksize);
					}
					dwPos += pksize;
				}
				dwMemPos = dwPos;
			}
			break;

		// "ENDE": end of file
		case 0x45444e45:
			goto dmfexit;
		
		// Unrecognized id, or "ENDE" field
		default:
			dwMemPos += 4;
			break;
		}
	}
dmfexit:
	if (!m_nChannels)
	{
		if (!m_nSamples)
		{
			m_nType = MOD_TYPE_NONE;
			return false;
		}
		m_nChannels = 4;
	}
	return true;
}


///////////////////////////////////////////////////////////////////////
// DMF Compression

#pragma pack(1)

typedef struct DMF_HNODE
{
	short int left, right;
	uint8_t value;
} DMF_HNODE;

typedef struct DMF_HTREE
{
	uint8_t * ibuf;
	uint8_t * ibufmax;
	uint32_t bitbuf;
	uint32_t bitnum;
	uint32_t lastnode, nodecount;
	DMF_HNODE nodes[256];
} DMF_HTREE;

#pragma pack()


// DMF Huffman ReadBits
uint8_t DMFReadBits(DMF_HTREE *tree, uint32_t nbits)
//-------------------------------------------
{
	uint8_t x = 0, bitv = 1;
	while (nbits--)
	{
		if (tree->bitnum)
		{
			tree->bitnum--;
		} else
		{
			tree->bitbuf = (tree->ibuf < tree->ibufmax) ? *(tree->ibuf++) : 0;
			tree->bitnum = 7;
		}
		if (tree->bitbuf & 1) x |= bitv;
		bitv <<= 1;
		tree->bitbuf >>= 1;
	}
	return x;
}

//
// tree: [8-bit value][12-bit index][12-bit index] = 32-bit
//

void DMFNewNode(DMF_HTREE *tree)
//------------------------------
{
	uint8_t isleft, isright;
	uint32_t actnode;

	actnode = tree->nodecount;
	if (actnode > 255) return;
	tree->nodes[actnode].value = DMFReadBits(tree, 7);
	isleft = DMFReadBits(tree, 1);
	isright = DMFReadBits(tree, 1);
	actnode = tree->lastnode;
	if (actnode > 255) return;
	tree->nodecount++;
	tree->lastnode = tree->nodecount;
	if (isleft)
	{
		tree->nodes[actnode].left = tree->lastnode;
		DMFNewNode(tree);
	} else
	{
		tree->nodes[actnode].left = -1;
	}
	tree->lastnode = tree->nodecount;
	if (isright)
	{
		tree->nodes[actnode].right = tree->lastnode;
		DMFNewNode(tree);
	} else
	{
		tree->nodes[actnode].right = -1;
	}
}


int DMFUnpack(uint8_t * psample, uint8_t * ibuf, uint8_t * ibufmax, uint32_t maxlen)
//----------------------------------------------------------------------
{
	DMF_HTREE tree;
	uint32_t actnode;
	uint8_t value, sign, delta = 0;
	
	memset(&tree, 0, sizeof(tree));
	tree.ibuf = ibuf;
	tree.ibufmax = ibufmax;
	DMFNewNode(&tree);
	value = 0;
	for (uint32_t i=0; i<maxlen; i++)
	{
		actnode = 0;
		sign = DMFReadBits(&tree, 1);
		do
		{
			if (DMFReadBits(&tree, 1))
				actnode = tree.nodes[actnode].right;
			else
				actnode = tree.nodes[actnode].left;
			if (actnode > 255) break;
			delta = tree.nodes[actnode].value;
			if ((tree.ibuf >= tree.ibufmax) && (!tree.bitnum)) break;
		} while ((tree.nodes[actnode].left >= 0) && (tree.nodes[actnode].right >= 0));
		if (sign) delta ^= 0xFF;
		value += delta;
		psample[i] = (i) ? value : 0;
	}
#ifdef DMFLOG
//	Log("DMFUnpack: %d remaining bytes\n", tree.ibufmax-tree.ibuf);
#endif
	return tree.ibuf - ibuf;
}


