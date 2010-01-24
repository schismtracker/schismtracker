#ifndef _XMDEFS_H_
#define _XMDEFS_H

#pragma pack(push, 1)

typedef struct tagXMFILEHEADER
{
        uint32_t size;
        uint16_t norder;
        uint16_t restartpos;
        uint16_t channels;
        uint16_t patterns;
        uint16_t instruments;
        uint16_t flags;
        uint16_t speed;
        uint16_t tempo;
        uint8_t order[256];
} XMFILEHEADER;


typedef struct tagXMINSTRUMENTHEADER
{
        uint32_t size;
        int8_t name[22];
        uint8_t type;
        uint8_t samples;
        uint8_t samplesh;
} XMINSTRUMENTHEADER;


typedef struct tagXMSAMPLEHEADER
{
        uint32_t shsize;
        uint8_t snum[96];
        uint16_t venv[24];
        uint16_t penv[24];
        uint8_t vnum, pnum;
        uint8_t vsustain, vloops, vloope, psustain, ploops, ploope;
        uint8_t vtype, ptype;
        uint8_t vibtype, vibsweep, vibdepth, vibrate;
        uint16_t volfade;
        uint16_t res;
        uint8_t reserved1[20];
} XMSAMPLEHEADER;

typedef struct tagXMSAMPLESTRUCT
{
        uint32_t samplen;
        uint32_t loopstart;
        uint32_t looplen;
        uint8_t vol;
        signed char finetune;
        uint8_t type;
        uint8_t pan;
        signed char relnote;
        uint8_t res;
        char name[22];
} XMSAMPLESTRUCT;

#pragma pack(pop)

#endif
