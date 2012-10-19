#include <stdio.h>
#include <math.h>
#include <string.h>
#include <sys/types.h>

#define RDS_AF_MAX			26
#define RDS_RT_MAX			256
#define RDS_PS_LEN			8
#define RDS_TP				(1 << 10)

#define RDS_GROUP_0A_TA			(1 << 4)
#define RDS_GROUP_0A_MUSIC		(1 << 3)
#define RDS_GROUP_0A_DI_EON		(1 << 0)
#define RDS_GROUP_0A_DI_COMP	(1 << 1)
#define RDS_GROUP_0A_DI_AHEAD	(1 << 2)
#define RDS_GROUP_0A_DI_STEREO	(1 << 3)

#define RDS_GROUP_TYPE(a, b)	((((a) & 0xf) << 1) | (b & 0x1))
#define RDS_GROUP(x)			(((x) & 0x1f) << 11)
#define RDS_PTY(x)				(((x) & 0x1f) << 5)

#define RDS_BLOCK_A			1
#define RDS_BLOCK_B			2
#define RDS_BLOCK_C			4
#define RDS_BLOCK_D			8

#define RDS_BLOCK_DATA_LEN	16
#define RDS_BLOCK_CRC_LEN	10
#define RDS_BLOCK_LEN		(RDS_BLOCK_DATA_LEN + RDS_BLOCK_CRC_LEN)

#define RDS_GROUP_NUMBLOCKS	4
#define RDS_GROUP_LEN		(RDS_BLOCK_LEN * RDS_GROUP_NUMBLOCKS)

#define RDS_TMC_AID			0xCD46
#define RDS_TMC_GROUP		RDS_GROUP_TYPE(8, 0)
#define RDS_TMC_T			(1 << 4)	/*  */
#define RDS_TMC_F			(1 << 3)	/*  */
#define RDS_TMC_DP(x)		((x) & 0x7)			/* duration and persistence */

#define RDS_TMC_D			(1 << 15)	/* diversion advice */
#define RDS_TMC_PN			(1 << 14)	/* direction */
#define RDS_TMC_EXTENT(x)	(((x) & 0x7) << 11)	/*  */
#define RDS_TMC_EVENT(x)	(x)			/* terrorist incident */
#define RDS_TMC_LOCATION(x)	(x)			/* Ilmenau-Ost */


#define RDS_TMC_VAR(x)	(((x) & 0x3) << 14)	/* Variant */

/* Variant 0 */
#define RDS_TMC_LTN(x)	(((x) & 0x3f) << 6)	/* Location Table Number */
#define RDS_TMC_AFI		(1 << 5)	/* AF Indicator */
#define RDS_TMC_M		(1 << 4)	/* Mode of Transmission */
/* if !M */
#define RDS_TMC_I		(1 << 3)	/* International */
#define RDS_TMC_N		(1 << 2)	/* National */
#define RDS_TMC_R		(1 << 1)	/* Regional */
#define RDS_TMC_U		(1 << 0)	/* Urban */

/* Variant 1 */
#define RDS_TMC_GAP(x)		(((x) & 0x3) << 12)	/* Gap parameter */
#define RDS_TMC_SID(x)		(((x) & 0x3f) << 6)	/* SID */
#define RDS_TMC_TA(x)		(((x) & 0x3) << 4)	/* Activity time */
#define RDS_TMC_TW(x)		(((x) & 0x3) << 2)	/* Window time */
#define RDS_TMC_TD(x)		(((x) & 0x3) << 0)	/* Delay time */



#define DIFF_ENCODE(in, ep) \
	(ep ^ in)
#define MANCH_ENCODE(clk, bit) \
	(((clk ^ bit) << 1) | ((clk ^ 0x1 ^ bit)))
#define NRZ(in) \
	(double)(((int)((in) & 0x1) << 1) - 1)

#define LOWPASS(in, out, k) \
	(out + k * (in - out))
#define HIGHPASS(in1, in2, out, k) \
	(k * (out + in1 - in2))

#define RISING_EDGE(clk, clkp) \
	(clkp <= 0.0 && clk > 0.0)
#define FALLING_EDGE(clk, clkp) \
	(clkp >= 0.0 && clk < 0.0)
#define PREEMP_IIR(sig, sigp, outp) \
	(5.30986 * sig + -4.79461 * sigp + 0.48475 * outp)

union sample_u {
	struct {
		float l, r;
	};
	float s[2];
};

struct rds_info_s {
	unsigned short pi;
	unsigned short af[RDS_AF_MAX];
	unsigned short di;
	char  pty;
	char  ps_name[RDS_PS_LEN];
	char  radiotext[RDS_RT_MAX];
};

struct rds_group_0a_s {
	short di;
	short af[RDS_AF_MAX];
	char  ps_name[RDS_PS_LEN];
};

struct rds_group_2a_s {
	char radiotext[RDS_RT_MAX];
	char rt_len;
};

struct rds_group_s {
	char  bits[RDS_GROUP_LEN];
	union {
		struct {
			unsigned short A, B, C, D;
		};
		unsigned short blocks[RDS_GROUP_NUMBLOCKS];
	};
	int info;
	void *p;
};

static const unsigned short rds_poly = 0x5B9;
static const unsigned short rds_checkwords[] = {
	0xFC, 0x198, 0x168, 0x1B4, 0x350, 0x0};


static struct rds_info_s rds_info = {
	0x10f0,
	{0xE210, 0x20E0},
	RDS_GROUP_0A_DI_STEREO,
	0x00,
	"<pirate>",
	"Stereo is the future!\r"};

static unsigned short rds_crc(unsigned short in) {
	int i;
	unsigned short reg = 0;
	
	for (i = 16; i > 0; i--)  {
		reg = (reg << 1) | ((in >> (i - 1)) & 0x1);
		if(reg & (1 << 10))
			reg = reg ^ rds_poly;
	}
	for (i = 10; i > 0; i--) {
		reg = reg << 1;
		if(reg & (1 << 10))
			reg = reg ^ rds_poly;
	}
	return (reg & ((1 << 10) - 1));
}

static void rds_bits_to_char(char *out, unsigned short in, int len) {
	int n = len;
	unsigned short mask;

	while(n--) {
		mask = 1 << n;
		out[len - 1 - n] = ((in & mask) >> n) & 0x1;
	}
}

static void rds_serialize(struct rds_group_s *group, char flags) {
	int n = 4;

	while(n--) {
		int start = RDS_BLOCK_LEN * n;
		unsigned short block = group->blocks[n];
		unsigned short crc;

		if(!((flags >> n) & 0x1))
			continue;

		crc  = rds_crc(block);
		crc ^= rds_checkwords[n];
		rds_bits_to_char(
			&group->bits[start],
			block,
			RDS_BLOCK_DATA_LEN);
		rds_bits_to_char(
			&group->bits[start + RDS_BLOCK_DATA_LEN],
			crc,
			RDS_BLOCK_CRC_LEN);
	}
}

static void rds_group_0A_init(
		struct rds_info_s *info, struct rds_group_s *group) {
	group->A = info->pi;
	group->B = RDS_GROUP(RDS_GROUP_TYPE(0, 0))
		| RDS_TP
		| RDS_PTY(info->pty)
		| RDS_GROUP_0A_MUSIC;
	group->info = ((info->af[0] >> 8) & 0xff) - 0xE0;

	rds_serialize(group, RDS_BLOCK_A);
}

static void rds_group_0A_update(
		struct rds_info_s *info, struct rds_group_s *group) {
	static int af_pos = 0, ps_pos = 0;
	unsigned short di = (info->di >> (ps_pos >> 1)) & 0x1;

	group->B = group->B & 0xfff8
		| di << 2
		| ps_pos >> 1;
	group->C = info->af[af_pos];
	group->D = info->ps_name[ps_pos] << 8
		| info->ps_name[ps_pos + 1];
	
	rds_serialize(group,
		RDS_BLOCK_B
		| RDS_BLOCK_C
		| RDS_BLOCK_D);
	
	af_pos = (af_pos + 1) % group->info;
	ps_pos = (ps_pos + 2) & (RDS_PS_LEN - 1);
}

static void rds_group_2A_init(
		struct rds_info_s *info, struct rds_group_s *group) {
	int n, len, pad;

	group->A = info->pi;
	group->B = RDS_GROUP(RDS_GROUP_TYPE(2, 0))
		| RDS_TP
		| RDS_PTY(info->pty);
	group->info = strnlen(info->radiotext, RDS_RT_MAX);

	rds_serialize(group, RDS_BLOCK_A);
}

static void rds_group_2A_update(
		struct rds_info_s *info, struct rds_group_s *group) {
	static int rt_pos = 0;
	static int b_pos = 0;

	group->B = group->B & 0xffe0 | b_pos;
	group->C = info->radiotext[rt_pos + 0] << 8
		| info->radiotext[rt_pos + 1];
	group->D = info->radiotext[rt_pos + 2] << 8
		| info->radiotext[rt_pos + 3];

	rds_serialize(group,
		RDS_BLOCK_B
		| RDS_BLOCK_C
		| RDS_BLOCK_D);

	b_pos = (b_pos + 1) & 0xf;
	if((rt_pos += 4) > group->info)
		rt_pos = 0, b_pos = 0;
}

static void rds_group_3A_init(
		struct rds_info_s *info, struct rds_group_s *group) {
	group->A = info->pi;
	group->B = RDS_GROUP(RDS_GROUP_TYPE(3, 0))
		| RDS_TP
		| RDS_PTY(info->pty)
		| RDS_TMC_GROUP;
	group->D = RDS_TMC_AID;

	rds_serialize(group,
		  RDS_BLOCK_A
		| RDS_BLOCK_B
		| RDS_BLOCK_D);
}

static void rds_group_3A_update(
		struct rds_info_s *info, struct rds_group_s *group) {
	static int toggle = 1;

	if(toggle)
		group->C = RDS_TMC_VAR(0)
			| RDS_TMC_LTN(1)
			| RDS_TMC_N
			| RDS_TMC_R;
	else
		group->C = RDS_TMC_VAR(1)
			| RDS_TMC_GAP(0)
			| RDS_TMC_SID(0)
			| RDS_TMC_TA(0)
			| RDS_TMC_TW(0)
			| RDS_TMC_TD(0);

	rds_serialize(group, RDS_BLOCK_C);

	toggle ^= 1;
}

static void rds_group_8A_init(
		struct rds_info_s *info, struct rds_group_s *group) {
	group->A = info->pi; 
	group->B = RDS_GROUP(RDS_GROUP_TYPE(8, 0))
		| RDS_TP
		| RDS_PTY(info->pty)
		| RDS_TMC_F
		| RDS_TMC_DP(0);
	group->C = RDS_TMC_D
		| RDS_TMC_PN
		| RDS_TMC_EXTENT(1)
		| RDS_TMC_EVENT(1478);
	group->D = RDS_TMC_LOCATION(12693);
	
	rds_serialize(group,
		  RDS_BLOCK_A
		| RDS_BLOCK_B
		| RDS_BLOCK_C
		| RDS_BLOCK_D);
}

static void rds_group_8A_update(
		struct rds_info_s *info, struct rds_group_s *group) {
}


static struct rds_group_s group_0a;
static struct rds_group_s group_2a;
static struct rds_group_s group_3a;
static struct rds_group_s group_8a;

static struct rds_group_s *rds_group_schedule() {
	static int ps = 1;
	struct rds_group_s *group;

	switch(ps) {
		case 0:
			rds_group_0A_update(&rds_info, &group_0a);
			group = &group_0a; break;
		case 1:
			rds_group_2A_update(&rds_info, &group_2a);
			group = &group_2a; break;
		case 2:
			rds_group_3A_update(&rds_info, &group_3a);
			group = &group_3a; break;
		case 3:
			rds_group_8A_update(&rds_info, &group_8a);
			group = &group_8a; break;
	}

	ps = (ps + 1) % 4;
	return group;
}

int main(int argc, char **argv) {
	union  sample_u samp;
	union  sample_u samp_s;
	union  sample_u samp_p = {0.0, 0.0};
	union  sample_u samp_sp = {0.0, 0.0};
	double out;
	float  out_f;
	double lmr;
	double mono;
	double mono_p = 0.0;
	double mono_s = 0.0;
	double mono_sp = 0.0;
	double sym;
	double sym_p = 0.0;
	double sym_s;
	char   m = 0;
	char   e = 0;
	char   e_p = 0;
	double bit_d = 0.0;
	int    bpos = 0;
	double symclk_p = 0.0;
	char   symclk_b = 1;
	int    n, len;
	unsigned long pos = 0;
	struct rds_group_s *group;

	rds_group_0A_init(&rds_info, &group_0a);
	rds_group_2A_init(&rds_info, &group_2a);
	rds_group_3A_init(&rds_info, &group_3a);
	rds_group_8A_init(&rds_info, &group_8a);

	group = rds_group_schedule();

	while(!feof(stdin)) {
		double clock   = 2.0 * M_PI * (double)(pos) * 19.0/192.0;
		double pilot   = sin(clock);
		double carrier = sin(clock * 2.0);
		double rds     = sin(clock * 3.0);
		double symclk  = sin(clock / 16.0);

		len = fread(&samp_s, 8, 1, stdin);
		if(len != 1)
			break;

		
		if(RISING_EDGE(symclk, symclk_p)) {
			e     = DIFF_ENCODE(group->bits[bpos], e_p);
			m     = MANCH_ENCODE(symclk_b, e);
			bit_d = NRZ(m);
			
			bpos  = (bpos + 1) % RDS_GROUP_LEN;
			e_p   = e;
			symclk_b ^= 1;

			if(!bpos) {
				group = rds_group_schedule();
				/* fprintf(stderr, "%.4x %.4x %.4x %.4x\n",
					group->A, group->B, group->C, group->D); */
				/* for(n = 0; n < RDS_GROUP_LEN; n++)
					putc('0' + group->bits[n], stderr); */
			}

			if(pos > 34560000l)
				pos = 0;
		}
		if(FALLING_EDGE(symclk, symclk_p)) {
			bit_d = NRZ(m >> 1);
			symclk_b ^= 1;
		}

		samp.l = PREEMP_IIR(samp_s.l, samp_sp.l, samp_p.l);
		samp.r = PREEMP_IIR(samp_s.r, samp_sp.r, samp_p.r);
		
		mono   = (samp.l + samp.r) / 2.0;
		lmr    =  samp.l - samp.r;	
		
		sym_s  = fabs(symclk) * bit_d;
		sym    = LOWPASS(sym_s, sym_p, 0.02);

		out  = 0.45  * mono;
		out += 0.10  * pilot;
		out += 0.225 * lmr * carrier;
		out += 0.07  * sym * rds;

		out_f = (float)(out);
		
		len = fwrite(&out_f, 4, 1, stdout);
		if(len != 1)
			break;

		symclk_p  = symclk;
		mono_p    = mono;
		mono_sp   = mono_s;
		sym_p     = sym;
		samp_sp.l = samp_s.l;
		samp_sp.r = samp_s.r;
		samp_p.l  = samp.l;
		samp_p.r  = samp.r;

		pos++;
	}

	return 1;
}

