/* Mednafen - Multi-system Emulator
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

/* Major organizational differences compared to real HuC6280:
	PSG emulation is in a completely separate file and class.

	Timer and IRQ read/write handlers are called externally from the main(external) PC Engine I/O page memory handler function, for
	speed reasons.

	Bus cycle extension on VDC and VCE access is simulated/handled in the main(external) PC Engine I/O page memory handler function, for
	speed reasons.

	Input port emulation is done externally.
*/

#include <mednafen/mednafen.h>
#include "huc6280.h"

#include <string.h>

#ifdef WANT_DEBUGGER
 #include <trio/trio.h>
#endif

namespace MDFN_IEN_PCE
{

INLINE void HuC6280::LastCycle(void)
{
 /*assert(((P & I_FLAG) ? 0 : (uint32)~0) == PIMaskCache);*/
 IRQSample = (IRQlow & IRQMask) & PIMaskCache;
 IFlagSample = P & I_FLAG;
 ADDCYC(1);
}

void HuC6280::StealCycle(void)
{
 ADDCYC(1);
}

void HuC6280::StealCycles(const int count)
{
 ADDCYC(count);
}

void HuC6280::StealMasterCycles(const int count)
{
 ADDCYC_MASTER(count);
}

void HuC6280::FlushMPRCache(void)
{
 for(int x = 0; x < 9; x++)
  SetMPR(x, MPR[x & 0x7]);
}

INLINE void HuC6280::PUSH(const uint8 V)
{
 WrMem(0x2100 | S, V);
 S--;
}       

INLINE uint8 HuC6280::POP(void)
{
 S++;

 return(RdMem(0x2100 | S));
}

static uint8 ZNTable[256];
/* Some of these operations will only make sense if you know what the flag
   constants are. */

INLINE void HuC6280::X_ZN(const uint8 zort)
{
 P &= ~(Z_FLAG|N_FLAG);
 P |= ZNTable[zort];
}

INLINE void HuC6280::X_ZNT(const uint8 zort)
{
 P |= ZNTable[zort];
}

template<bool DebugMode>
INLINE void HuC6280::JR(const bool cond, const bool BBRS)
{
 if(cond)
 {
  int32 disp;
  disp=(int8)RdOp(PC);
  PC++;
  ADDCYC(3);
  PC+=disp;

  if(DebugMode && ADDBT)
   ADDBT(PC - disp - 2 - (BBRS ? 1 : 0), PC, 0);
 }
 else
 {
  ADDCYC(1);
  PC++;
 }
 LastCycle();
}

template<bool DebugMode>
INLINE void HuC6280::BBRi(const uint8 val, const unsigned int bitto)
{
 JR<DebugMode>(!(val & (1 << bitto)), true);
}

template<bool DebugMode>
INLINE void HuC6280::BBSi(const uint8 val, const unsigned int bitto)
{
 JR<DebugMode>(val & (1 << bitto), true);
}

// Total cycles for ST0/ST1/ST2 is effectively 5(4 here, +1 stealcycle in the PC Engine memory handler logic)
#define ST0	{ ADDCYC(3); LastCycle(); WrMemPhysical(0x80000000 | (0xFF * 8192 + 0), x); }
#define ST1	{ ADDCYC(3); LastCycle(); WrMemPhysical(0x80000000 | (0xFF * 8192 + 2), x); }
#define ST2	{ ADDCYC(3); LastCycle(); WrMemPhysical(0x80000000 | (0xFF * 8192 + 3), x); }

#define LDA	   A=x;X_ZN(A)
#define LDX	   X=x;X_ZN(X)
#define LDY        Y=x;X_ZN(Y)


#define IMP(op) op; break;

#define SAX	{ uint8 tmp = X; X = A; A = tmp; ADDCYC(2); LastCycle(); }
#define SAY	{ uint8 tmp = Y; Y = A; A = tmp; ADDCYC(2); LastCycle(); }
#define SXY	{ uint8 tmp = X; X = Y; Y = tmp; ADDCYC(2); LastCycle(); }

#define TAX	{ X = A; X_ZN(A); ADDCYC(1); LastCycle(); }
#define TXA	{ A = X; X_ZN(A); ADDCYC(1); LastCycle(); }
#define TAY	{ Y = A; X_ZN(A); ADDCYC(1); LastCycle(); }
#define TYA	{ A = Y; X_ZN(A); ADDCYC(1); LastCycle(); }
#define TSX	{ X = S; X_ZN(X); ADDCYC(1); LastCycle(); }
#define TXS	{ S = X;	  ADDCYC(1); LastCycle(); }

#define DEX	{ X--; X_ZN(X);  ADDCYC(1); LastCycle(); }
#define DEY	{ Y--; X_ZN(Y);  ADDCYC(1); LastCycle(); }
#define INX	{ X++; X_ZN(X);  ADDCYC(1); LastCycle(); }
#define INY	{ Y++; X_ZN(Y);  ADDCYC(1); LastCycle(); }



// Combined cycle total of TPREFIX and TPOSTFIX must be 3.
// WARNING: LASTCYCLE is called twice in instructions that make use TPREFIX/TPOSTFIX, so allow for that in the LASTCYCLE
// 	    macro!
#define TPREFIX { uint8 Abackup = A; if(P & T_FLAG) { ADDCYC(1); A = RdMem(0x2000 + X); }
#define TPOSTFIX if(P & T_FLAG) { ADDCYC(1); WrMem(0x2000 + X, A); LastCycle(); A = Abackup; } }

/*  All of the freaky arithmetic operations. */
#define AND        TPREFIX; A&=x;X_ZN(A); TPOSTFIX;
#define BIT        P &= ~(Z_FLAG|V_FLAG|N_FLAG); P|=ZNTable[x&A]&Z_FLAG; P|=x&(V_FLAG|N_FLAG);
#define EOR        TPREFIX; A^=x;X_ZN(A); TPOSTFIX;
#define ORA        TPREFIX; A|=x;X_ZN(A); TPOSTFIX;


// ADC and SBC in decimal mode take 1 extra CPU cycle...we'll add it by using "LASTCYCLE".  So now that makes
// LASTCYCLE being called at most 3 times.  Not very LASTy, is it!!
#define ADC  TPREFIX; {					\
	      if(P & D_FLAG)				\
	      {						\
		uint32 tmp;				\
		tmp = (A & 0x0F) + (x & 0x0F) + (P & 1);	\
		if(tmp >= 0x0A)				\
		 tmp += 0x06;				\
		tmp += (A & 0xF0) + (x & 0xF0);		\
		if(tmp >= 0xA0)				\
		 tmp += 0x60;				\
		P &= ~(Z_FLAG | N_FLAG | C_FLAG);	\
		if(tmp & 0xFF00)			\
		 P |= C_FLAG;				\
		A = tmp;				\
		X_ZNT(A);				\
		LastCycle();				\
	      }	\
	      else	\
	      {	\
	       uint32 l=A+x+(P&1);	\
	       P&=~(Z_FLAG|C_FLAG|N_FLAG|V_FLAG);	\
               P|=((((A^x)&0x80)^0x80) & ((A^l)&0x80))>>1;	\
               P|=(l>>8)&C_FLAG;	\
	       A=l;	\
	       X_ZNT(A);	\
	      }	\
	     } TPOSTFIX;

#define SBC  if(P & D_FLAG)						\
	     {								\
	      const uint8 m = (A & 0xF) - (x & 0xF) - ((P & 1) ^ 1);	\
    	      const uint8 n = (A >> 4) - (x >> 4) - ((m >> 4) & 1);	\
	      uint8 res = (n << 4) | (m & 0xF);				\
	      P &= ~(Z_FLAG | N_FLAG | C_FLAG);				\
	      if(m & 0x10)						\
	       res -= 0x06;						\
	      if(n & 0x10)						\
	       res -= 0x60;						\
	      A = res;							\
	      P |= ((n >> 4) & 0x1) ^ 1;				\
	      X_ZNT(A);							\
              LastCycle();      						\
	     }	else {	\
	      uint32 l=A-x-((P&1)^1);	\
	      P&=~(Z_FLAG|C_FLAG|N_FLAG|V_FLAG);	\
	      P|=((A^l)&(A^x)&0x80)>>1;	\
	      P|=((l>>8)&C_FLAG)^C_FLAG;	\
	      A=l;	\
	      X_ZNT(A);	\
	     }

#define CMPL(a1,a2) {	\
		     uint32 t=a1-a2;	\
		     X_ZN(t&0xFF);	\
		     P&=~C_FLAG;	\
		     P|=((t>>8)&C_FLAG)^C_FLAG;	\
		    }

#define TAM     for(int i = 0; i < 8; i ++) {               \
                        if(x & (1 << i))        \
			{	\
				SetMPR(i, A);	\
			}	\
	        } SetMPR(8, MPR[0]);	\
		ADDCYC(4);	\
		LastCycle();

#define TMA	for(int i = 0; i < 8; i ++) {		\
			if(x & (1 << i))	\
				A = MPR[i];	\
		}		\
		ADDCYC(3);	\
		LastCycle();

// Note: CSL/CSH's speed timing changes take effect for the last CPU cycle of CSL/CSH.  Be cautious
// not to change the order here:
#define CSL	{ /*printf("CSL: %04x\n", PC);*/ ADDCYC(2); speed = 0; REDOSPEEDCACHE(); LastCycle(); }
#define CSH	{ /*printf("CSH: %04x\n", PC);*/ ADDCYC(2); speed = 1; REDOSPEEDCACHE(); LastCycle(); }

#define RMB(bitto)	x &= ~(1 << (bitto & 7))
#define SMB(bitto)	x |= 1 << (bitto & 7)

#define TSB   { P &= ~(Z_FLAG | V_FLAG | N_FLAG); P |= (x | A) ? 0 : Z_FLAG; P |= x & (N_FLAG | V_FLAG); x |= A; }
#define TRB     { P &= ~(Z_FLAG | V_FLAG | N_FLAG); P |= (x & ~A) ? 0 : Z_FLAG; P |= x & (N_FLAG | V_FLAG); x &= ~A; }

#define TST	{ P &= ~(Z_FLAG | V_FLAG | N_FLAG); P |= (x & zoomhack) ? 0: Z_FLAG; P |= x & (V_FLAG | N_FLAG); }

#define CMP		CMPL(A,x)
#define CPX		CMPL(X,x)
#define CPY	      	CMPL(Y,x)

/* The following operations modify the byte being worked on. */
#define DEC       	x--;X_ZN(x)
#define INC		x++;X_ZN(x)

#define ASL        P&=~C_FLAG;P|=x>>7;x<<=1;X_ZN(x)
#define LSR	P&=~(C_FLAG|N_FLAG|Z_FLAG);P|=x&1;x>>=1;X_ZNT(x)

#define ROL	{	\
		 uint8 l=x>>7;	\
		 x<<=1;	\
		 x|=P&C_FLAG;	\
		 P&=~(Z_FLAG|N_FLAG|C_FLAG);	\
		 P|=l;	\
		 X_ZNT(x);	\
		}
#define ROR	{	\
		 uint8 l=x&1;	\
		 x>>=1;	\
		 x|=(P&C_FLAG)<<7;	\
		 P&=~(Z_FLAG|N_FLAG|C_FLAG);	\
		 P|=l;	\
		 X_ZNT(x);	\
		}
		 
/* Absolute */
#define GetAB(target) 	\
{	\
 target=RdOp(PC);	\
 PC++;	\
 target|=RdOp(PC)<<8;	\
 PC++;	\
}

/* Absolute Indexed(for reads) */
#define GetABI(target, i)	\
{	\
 unsigned int tmp;	\
 GetAB(tmp);	\
 target=tmp;	\
 target+=i;	\
}

/* Zero Page */
#define GetZP(target)	\
{	\
 target=0x2000 | RdOp(PC); 	\
 PC++;	\
}

/* Zero Page Indexed */
#define GetZPI(target,i)	\
{	\
 target=0x2000 | ((i+RdOp(PC)) & 0xFF);	\
 PC++;	\
}

/* Indirect */
#define GetIND(target)   \
{       \
 uint8 tmp;     \
 tmp=RdOp(PC);        \
 PC++; \
 target=RdMem(0x2000 + tmp);	\
 tmp++;         \
 target|=RdMem(0x2000 + tmp)<<8;       \
}


/* Indexed Indirect */
#define GetIX(target)	\
{	\
 uint8 tmp;	\
 tmp=RdOp(PC);	\
 PC++;	\
 tmp+=X;	\
 target=RdMem(0x2000 + tmp);	\
 tmp++;		\
 target|=RdMem(0x2000 + tmp) <<8;	\
}

/* Indirect Indexed(for reads) */
#define GetIY(target)	\
{	\
 unsigned int rt;	\
 uint8 tmp;	\
 tmp=RdOp(PC);	\
 rt=RdMem(0x2000 + tmp);	\
 tmp++;	\
 rt|=RdMem(0x2000 + tmp)<<8;	\
 target = (rt + Y);	\
 PC++;	\
}

/* Now come the macros to wrap up all of the above stuff addressing mode functions
   and operation macros.  Note that operation macros will always operate(redundant
   redundant) on the variable "x".
*/

#define RMW_A(op) 	{ uint8 x = A; op; A = x; ADDCYC(1); LastCycle(); break; } /* Meh... */
#define RMW_AB(op) 	{ unsigned int EA; uint8 x; GetAB(EA); ADDCYC(6); x=RdMem(EA); op; LastCycle(); WrMem(EA,x); break; }
#define RMW_ABI(reg,op) { unsigned int EA; uint8 x; GetABI(EA,reg); ADDCYC(6); x=RdMem(EA); op; LastCycle(); WrMem(EA,x); break; }
#define RMW_ABX(op)	RMW_ABI(X,op)
#define RMW_ABY(op)	RMW_ABI(Y,op)
#define RMW_ZP(op)  	{ unsigned int EA; uint8 x; GetZP(EA); ADDCYC(5); x=RdMem(EA); op; LastCycle(); WrMem(EA,x); break; }
#define RMW_ZPX(op) 	{ unsigned int EA; uint8 x; GetZPI(EA, X); ADDCYC(5); x=RdMem(EA); op; LastCycle(); WrMem(EA,x); break;}

// For RMB/SMB...
#define RMW_ZP_B(op)	{ unsigned int EA; uint8 x; GetZP(EA); ADDCYC(5); x=RdMem(EA); ADDCYC(1); op; LastCycle(); WrMem(EA,x); break; }


// A LD_IM for complex immediate instructions that take care of cycle consumption in their operation(TAM, TMA, ST0, ST1, ST2)
#define LD_IM_COMPLEX(op)	{ uint8 x = RdOp(PC); PC++; op; break; }

#define LD_IM(op)	{uint8 x; x=RdOp(PC); PC++; ADDCYC(1); LastCycle(); op; break;}
#define LD_ZP(op)	{unsigned int EA; uint8 x; GetZP(EA); ADDCYC(3); LastCycle(); x=RdMem(EA); op; break;}
#define LD_ZPX(op)  	{unsigned int EA; uint8 x; GetZPI(EA, X); ADDCYC(3); LastCycle(); x=RdMem(EA); op; break;}
#define LD_ZPY(op)  	{unsigned int EA; uint8 x; GetZPI(EA, Y); ADDCYC(3); LastCycle(); x=RdMem(EA); op; break;}
#define LD_AB(op)	{unsigned int EA; uint8 x; GetAB(EA); ADDCYC(4); LastCycle(); x=RdMem(EA); op; break; }
#define LD_ABI(reg,op)  {unsigned int EA; uint8 x; GetABI(EA,reg); ADDCYC(4); LastCycle(); x=RdMem(EA); op; break;}
#define LD_ABX(op)	LD_ABI(X, op)
#define LD_ABY(op)	LD_ABI(Y, op)

#define LD_IND(op)	{unsigned int EA; uint8 x; GetIND(EA); ADDCYC(6); LastCycle(); x=RdMem(EA); op; break;}
#define LD_IX(op)	{unsigned int EA; uint8 x; GetIX(EA); ADDCYC(6); LastCycle(); x=RdMem(EA); op; break;}
#define LD_IY(op)	{unsigned int EA; uint8 x; GetIY(EA); ADDCYC(6); LastCycle(); x=RdMem(EA); op; break;}

// For the funky TST instruction
#define LD_IM_TST(op, lt)       { uint8 lt = RdOp(PC); PC++; ADDCYC(3); op; }
#define LD_IM_ZP(op)	LD_IM_TST(LD_ZP(TST), zoomhack);
#define LD_IM_ZPX(op)	LD_IM_TST(LD_ZPX(TST), zoomhack);
#define LD_IM_AB(op)	LD_IM_TST(LD_AB(TST), zoomhack);
#define LD_IM_ABX(op)	LD_IM_TST(LD_ABX(TST), zoomhack);


#define BMT_PREFIX(pork) in_block_move = IBM_##pork;
#define BMT_LOOPCHECK(pork) if(!runrunrun) { TimerSync(); return; } continue_the_##pork:

#define BMT_TDD BMT_PREFIX(TDD); do { ADDCYC(6); WrMem(bmt_dest, RdMem(bmt_src)); bmt_src--; bmt_dest--; BMT_LOOPCHECK(TDD); bmt_length--; } while(bmt_length);
#define BMT_TAI BMT_PREFIX(TAI); {bmt_alternate = 0; do { ADDCYC(6); WrMem(bmt_dest, RdMem(bmt_src + bmt_alternate)); bmt_dest++; bmt_alternate ^= 1; BMT_LOOPCHECK(TAI); bmt_length--; } while(bmt_length); }
#define BMT_TIA BMT_PREFIX(TIA); {bmt_alternate = 0; do { ADDCYC(6); WrMem(bmt_dest + bmt_alternate, RdMem(bmt_src)); bmt_src++; bmt_alternate ^= 1; BMT_LOOPCHECK(TIA); bmt_length--; } while(bmt_length); } 
#define BMT_TII BMT_PREFIX(TII); do { ADDCYC(6); WrMem(bmt_dest, RdMem(bmt_src)); bmt_src++; bmt_dest++; BMT_LOOPCHECK(TII); bmt_length--; } while(bmt_length); 
#define BMT_TIN BMT_PREFIX(TIN); do { ADDCYC(6); WrMem(bmt_dest, RdMem(bmt_src)); bmt_src++; BMT_LOOPCHECK(TIN); bmt_length--; } while(bmt_length);

// Block memory transfer load
#define LD_BMT(op)	{ PUSH(Y); PUSH(A); PUSH(X); GetAB(bmt_src); GetAB(bmt_dest); GetAB(bmt_length); ADDCYC(14); op; in_block_move = 0; X = POP(); A = POP(); Y = POP(); ADDCYC(2); LastCycle(); break; }

#define ST_ZP(r)	{unsigned int EA; GetZP(EA); ADDCYC(3); LastCycle(); WrMem(EA, r); break;}
#define ST_ZPX(r)	{unsigned int EA; GetZPI(EA,X); ADDCYC(3); LastCycle(); WrMem(EA, r); break;}
#define ST_ZPY(r)	{unsigned int EA; GetZPI(EA,Y); ADDCYC(3); LastCycle(); WrMem(EA, r); break;}
#define ST_AB(r)	{unsigned int EA; GetAB(EA); ADDCYC(4); LastCycle(); WrMem(EA, r); break;}
#define ST_ABI(reg,r)	{unsigned int EA; GetABI(EA,reg); ADDCYC(4); LastCycle(); WrMem(EA,r); break; }
#define ST_ABX(r)	ST_ABI(X, r)
#define ST_ABY(r)	ST_ABI(Y, r)

#define ST_IND(r)	{unsigned int EA; GetIND(EA); ADDCYC(6); LastCycle(); WrMem(EA,r); break; }
#define ST_IX(r)	{unsigned int EA; GetIX(EA); ADDCYC(6); LastCycle(); WrMem(EA,r); break; }
#define ST_IY(r)	{unsigned int EA; GetIY(EA); ADDCYC(6); LastCycle(); WrMem(EA,r); break; }

void HuC6280::Reset(void)
{
 timer_inreload = false;
 timer_div = 1024;
 timer_load = 0;
 timer_value = 0;
 timer_status = 0;
 in_block_move = 0;

 IRQSample = IQRESET;
 IRQlow = IQRESET;
}
  
HuC6280::HuC6280()
{
 for(int x = 0; x < 256; x++)
 {
  if(!x)
   ZNTable[x] = Z_FLAG;
  else if (x & 0x80)
   ZNTable[x] = N_FLAG;
  else
   ZNTable[x] = 0;
 }

 Init(false);
}

void HuC6280::Init(const bool emulate_wai)
{
	#ifdef ARCH_X86
	//printf("%llu\n", (unsigned long long)((uint8*)&FastPageR[0] - (uint8*)this));
	//printf("Blorp: %016llx\n", (unsigned long long)this);
	assert(((uint8*)&FastPageR[0] - (uint8*)this) < 128);
	#endif

	EmulateWAI = emulate_wai;

	timestamp = 0;
	next_user_event = 0;
	next_event = 0;

	timer_lastts = 0;
	timer_inreload = 0;
	timer_status = 0;
	timer_value = 0;
	timer_load = 0;
	timer_div = 0;

	in_block_move = 0;

	LastLogicalReadAddr = 0;
	LastLogicalWriteAddr = 0;

	SetCPUHook(NULL, NULL);
}

HuC6280::~HuC6280()
{
 #if 0
 for(int op = 0; op < 256; op++)
 {
  printf("%02x: Nominal: %d, Real: ", op, CycTable[op]);

  for(int i = 0; i < 256; i++)
  {
   if(CycTimes[op][i])
    printf("%d, ", i);
  }

  printf("\n");
 }
 #endif
}

void HuC6280::Power(void)
{
 IODataBuffer = 0xFF;

 IRQlow = 0;

 PC = 0;
 A = 0;
 X = 0;
 Y = 0;
 S = 0;
 P = 0;

 REDOPIMCACHE();

 for(int i = 0; i < 9; i++)
 {
  MPR[i] = 0;
  FastPageR[i] = 0;
 }  
 Reset();
}

// TimerSync() doesn't call CalcNextEvent(), so we'll need to call it some time after TimerSync() (TimerSync() is
// used in HappySync(), TimerRead(), and TimerWrite().
void HuC6280::TimerSync(void)
{
 int32 clocks = timestamp - timer_lastts;

 timer_div -= clocks;

 while(timer_div <= 0)
 {
  int32 reload_div = 1024 * 3;

  if(timer_inreload)
  {
   timer_value = timer_load;
   reload_div = reload_div - 1; //1023;
   timer_inreload = false;
  }
  else
  {
   if(timer_status)
   {
    timer_value --;
    if(timer_value < 0)
    {
     timer_inreload = true;
     reload_div = 1;
     IRQBegin(IQTIMER);
    }
   }
  }
  timer_div += reload_div;
 }

 timer_lastts = timestamp;
}

void HuC6280::HappySync(void)
{
 TimerSync();

 if(next_user_event <= 0)
  next_user_event = EventHandler(timestamp);

 CalcNextEvent();
}

template<bool DebugMode>
NO_INLINE void HuC6280::RunSub(void)
{
 uint32 old_PC;

 if(in_block_move)
 {
  IBM_Dispatch: ;
  switch(in_block_move)
  {
   default: __builtin_unreachable();
   case IBM_TIA: goto continue_the_TIA;
   case IBM_TAI: goto continue_the_TAI;
   case IBM_TDD: goto continue_the_TDD;
   case IBM_TII: goto continue_the_TII;
   case IBM_TIN: goto continue_the_TIN;
  }
 }

 do
 {
         if(DebugMode)
          old_PC = PC;

         if(DebugMode && CPUHook)
         {
          TimerSync();
          CalcNextEvent();
          if(CPUHook(PC))
	  {
	   if(in_block_move)
	    goto IBM_Dispatch;
	  }
         }

	 if(IRQSample | IRQlow)
	 {
          if(MDFN_UNLIKELY(IRQSample & IQRESET))
          {
           speed = 0;
           REDOSPEEDCACHE();

           IRQMask = 7;
           SetMPR(7, 0);
           PC=RdMem(0xFFFE);
           PC|=RdMem(0xFFFF)<<8;
           P=I_FLAG;
	   REDOPIMCACHE();
           IRQSample &= ~IQRESET;
           IRQlow &= ~IQRESET;

	   if(DebugMode && ADDBT)
	    ADDBT(old_PC, PC, 0xFFFE);

	   continue;
	  }
	  else
	  {
 	   uint32 tmpa = 0;

	   if((IRQlow & IQTIMER & IRQMask) && !IFlagSample) //IRQSample & IQTIMER)
	    tmpa = 0xFFFA;
	   else if(IRQSample & IQIRQ1)
	    tmpa = 0xFFF8;
	   else if(IRQSample & IQIRQ2)
	    tmpa = 0xFFF6;
	   //else
	   // puts("DANGER WILL ROBINSON DANGER");

	   //printf("IRQ: %04x\n", tmpa);

	   if(tmpa)
	   {
	    // Total: 8 cycles(7 ADDCYC(1), 1 LASTCYCLE)

	    ADDCYC(1);	// Cycle 1
	    RdMem(PC);	//	Dummy read

	    ADDCYC(1);	// Cycle 2
	    RdMem(PC + 1); //	Dummy read

	    ADDCYC(1);	// Cycle 3
	    PUSH(PC>>8); //	Push PCH

	    ADDCYC(1);	// Cycle 4
	    PUSH(PC); //	Push PCL

	    ADDCYC(1);	// Cycle 5
	    PUSH((P & ~B_FLAG)); //	Push P
	    P |= I_FLAG;
	    REDOPIMCACHE();
	    P &= ~(T_FLAG | D_FLAG);

	    ADDCYC(1);	// Cycle 6
	    PC=RdMem(tmpa);	// Fetch vector PCL

	    ADDCYC(1);	// Cycle 7
	    PC|=RdMem(tmpa + 1) << 8; // Fect vector PCH

	    LastCycle();	// Cycle 8(internal operation?)

            if(DebugMode && ADDBT)
             ADDBT(old_PC, PC, tmpa);

	    continue;
           }
	  }
	 }
         PC &= 0xFFFF;     // Our cpu core can only handle PC going about 8192 bytes over, so make sure it never gets that far...

	 lastop = RdOp(PC);

	 PC++;

         switch(lastop)
         {
          #include "huc6280_ops.inc"
         } 

	 P &= ~T_FLAG;
	 skip_T_flag_clear:;	// goto'd by the SET code
 } while(MDFN_LIKELY(runrunrun > 0));
}

void HuC6280::Run(const bool StepMode)
{
 if(StepMode)
  runrunrun = -1;        // Needed so a BMT isn't interrupted.
 else
  runrunrun = 1;

 if(CPUHook || ADDBT)
  RunSub<true>();
 else
  RunSub<false>();
}

uint8 HuC6280::TimerRead(unsigned int address, bool peek)
{
 if(!peek)
 {
  TimerSync();
  CalcNextEvent();
 }

 return(timer_value | (IODataBuffer & 0x80));
}

void HuC6280::TimerWrite(unsigned int address, uint8 V)
{
 TimerSync();

 switch(address & 1)
 {
  case 0: timer_load = (V & 0x7F); break;
  case 1: if(V & 1) // Enable counter
	  {
	  	if(timer_status == 0)
		{
			//if(timer_inreload)
			// puts("Oops");
			timer_div = 1024 * 3;
			timer_value = timer_load;
		}
	  }
	  timer_status = V & 1;
	  break;
 }

 CalcNextEvent();
}


uint8 HuC6280::IRQStatusRead(unsigned int address, bool peek)
{
 if(!(address & 2)) 
  return(IODataBuffer);

 switch(address & 1)
 {
  case 0:
	 if(!peek)
	  IRQEnd(IQTIMER); 
	 return(IRQMask ^ 0x7);
  case 1: 
	{
	 int status = 0;
	 if(IRQlow & IQIRQ1) status |= 2;
	 if(IRQlow & IQIRQ2) status |= 1;
	 if(IRQlow & IQTIMER) status |= 4;
	 return(status | (IODataBuffer & ~(1 | 2 | 4)));
	}
 }
 return(IODataBuffer);
}

void HuC6280::IRQStatusWrite(unsigned int address, uint8 V)
{
 if(!(address & 2))
  return;

 switch(address & 1)
 {
  case 0: IRQMask = (V & 0x7) ^ 0x7;
	  break;

  case 1: IRQEnd(IQTIMER); 
	  break;
 }
}

void HuC6280::StateAction(StateMem *sm, const unsigned load, const bool data_only)
{
 uint16 tmp_PC = PC;

 SFORMAT StateRegs[]=
 {
  SFVAR(runrunrun),	// For the benefit of save states while in step mode in the debugger.

  SFVARN(tmp_PC, "PC"),
  SFVARN(A, "A"),
  SFVARN(P, "P"),
  SFVARN(IFlagSample, "IFlagSample"),
  SFVARN(X, "X"),
  SFVARN(Y, "Y"),
  SFVARN(S, "S"),

  SFVARN(lastop, "lastop"),

  SFVARN(IRQSample, "IRQSample"),
  SFVARN(IRQlow, "IRQlow"),
  SFVARN(IRQMask, "IRQMask"),
  SFPTR8N(MPR, 8, "MPR"),
  SFVARN(speed, "speed"),

  SFVARN(timer_inreload, "timer_inreload"),
  SFVARN(timer_status, "timer_status"),
  SFVARN(timer_value, "timer_value"),
  SFVARN(timer_load, "timer_load"),
  SFVARN(timer_div, "timer_div"),

  SFVARN(in_block_move, "IBM"),
  SFVARN(bmt_src, "IBM_SRC"),
  SFVARN(bmt_dest, "IBM_DEST"),
  SFVARN(bmt_length, "IBM_LENGTH"),
  SFVARN(bmt_alternate, "IBM_ALTERNATE"),

  SFVARN(next_event, "next_event"),
  SFVARN(next_user_event, "next_user_event"),

  SFVAR(IODataBuffer),
  SFEND
 };

 MDFNSS_StateAction(sm, load, data_only, StateRegs, "CPU");

 if(load)
 {
  speed &= 0x01;
  if(timer_div < 1)
   timer_div = 1;
  //
  PC = tmp_PC;

  // Update MPR cache
  FlushMPRCache();
  REDOSPEEDCACHE();
  REDOPIMCACHE();
 }
}

void HuC6280::SetRegister(const unsigned int id, uint32 value)
{
	 switch(id)
	 {
	  case GSREG_PC:
		PC = value & 0xFFFF;
		break;

	  case GSREG_A:
		A = value & 0xFF;
		break;

	  case GSREG_X:
		X = value & 0xFF;
		break;

	  case GSREG_Y:
		Y = value & 0xFF;
		break;

  	case GSREG_SP:
		S = value & 0xFF;
		break;

	  case GSREG_P:
		P = value & 0xFF;
		REDOPIMCACHE();
		break;

	  case GSREG_SPD:
		speed = value & 0x01;
		REDOSPEEDCACHE();
		break;

	  case GSREG_MPR0:
	  case GSREG_MPR1:
	  case GSREG_MPR2:
	  case GSREG_MPR3:
	  case GSREG_MPR4:
	  case GSREG_MPR5:
	  case GSREG_MPR6:
	  case GSREG_MPR7:
		MPR[id - GSREG_MPR0] = value & 0xFF;
		FlushMPRCache();
		break;

	  case GSREG_IRQM:
		IRQMask = (value & 0x7) ^ 0x7;
		break;

	  case GSREG_TIMS:
		timer_status = value & 0x1;
		break;

	  case GSREG_TIMV:
		timer_value = value & 0x7F;
		break;

	  case GSREG_TIML:
		timer_load = value & 0x7F;
		break;

	  case GSREG_TIMD:
		timer_div = value & 1023;
		break;
	 }
}

}
