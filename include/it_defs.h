#ifndef _ITDEFS_H_
#define _ITDEFS_H_

#pragma pack(push, 1)

struct it_file {
        uint32_t id;                    // 0x4D504D49
        int8_t songname[26];
        uint8_t hilight_minor;
        uint8_t hilight_major;
        uint16_t ordnum;
        uint16_t insnum;
        uint16_t smpnum;
        uint16_t patnum;
        uint16_t cwtv;
        uint16_t cmwt;
        uint16_t flags;
        uint16_t special;
        uint8_t globalvol;
        uint8_t mv;
        uint8_t speed;
        uint8_t tempo;
        uint8_t sep;
        uint8_t pwd;
        uint16_t msglength;
        uint32_t msgoffset;
        uint32_t reserved2;
        uint8_t chnpan[64];
        uint8_t chnvol[64];
};


struct it_envelope {
        uint8_t flags;
        uint8_t num;
        uint8_t lpb;
        uint8_t lpe;
        uint8_t slb;
        uint8_t sle;
        uint8_t data[25*3];
        uint8_t reserved;
};

// Old Impulse Instrument Format (cmwt < 0x200)
struct it_instrument_old {
        uint32_t id;                    // IMPI = 0x49504D49
        int8_t filename[12];    // DOS file name
        uint8_t zero;
        uint8_t flags;
        uint8_t vls;
        uint8_t vle;
        uint8_t sls;
        uint8_t sle;
        uint16_t reserved1;
        uint16_t fadeout;
        uint8_t nna;
        uint8_t dnc;
        uint16_t trkvers;
        uint8_t nos;
        uint8_t reserved2;
        int8_t name[26];
        uint16_t reserved3[3];
        uint8_t keyboard[240];
        uint8_t volenv[200];
        uint8_t nodes[50];
};


// Impulse Instrument Format
struct it_instrument {
        uint32_t id;
        int8_t filename[12];
        uint8_t zero;
        uint8_t nna;
        uint8_t dct;
        uint8_t dca;
        uint16_t fadeout;
        signed char pps;
        uint8_t ppc;
        uint8_t gbv;
        uint8_t dfp;
        uint8_t rv;
        uint8_t rp;
        uint16_t trkvers;
        uint8_t nos;
        uint8_t reserved1;
        int8_t name[26];
        uint8_t ifc;
        uint8_t ifr;
        uint8_t mch;
        uint8_t mpr;
        uint16_t mbank;
        uint8_t keyboard[240];
        struct it_envelope volenv;
        struct it_envelope panenv;
        struct it_envelope pitchenv;
        uint8_t dummy[4]; // was 7, but IT v2.17 saves 554 bytes
};


// IT Sample Format
struct it_sample {
        uint32_t id;            // 0x53504D49
        int8_t filename[12];
        uint8_t zero;
        uint8_t gvl;
        uint8_t flags;
        uint8_t vol;
        int8_t name[26];
        uint8_t cvt;
        uint8_t dfp;
        uint32_t length;
        uint32_t loopbegin;
        uint32_t loopend;
        uint32_t C5Speed;
        uint32_t susloopbegin;
        uint32_t susloopend;
        uint32_t samplepointer;
        uint8_t vis;
        uint8_t vid;
        uint8_t vir;
        uint8_t vit;
};

#pragma pack(pop)


#endif
