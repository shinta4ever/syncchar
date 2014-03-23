#ifndef OPCODES_H
#define OPCODES_H

#define NEED_EXCLUSIVE 0x80

/* DEP 7/9/08 - Simics seems to barf in the custom instruction decoder
 * while running stamp.  Use magic instructions until this problem is
 * fixed.  */
//#define USE_MAGIC_TXOPCODES 1
#ifdef USE_MAGIC_TXOPCODES

#define OSA_OP_XGETTXID             1206
#define OSA_OP_XBEGIN               1207
#define OSA_OP_XBEGIN_IO            1208
#define OSA_OP_XEND                 1209
#define OSA_OP_XRESTART             1210
#define OSA_OP_XRESTART_EX          1211
#define OSA_OP_XCAS                 1212
#define OSA_OP_XPUSH                1213
#define OSA_OP_XPOP                 1214
#define OSA_OP_XSTATUS              1215

static __inline__ void _xpush() {
  magic_instruction(OSA_OP_XPUSH);
}

static __inline__ void _xpop() {
  magic_instruction(OSA_OP_XPOP);
}

static __inline__ unsigned int _xgettxid() {
   unsigned int ret;
   __asm__ __volatile__ ("xchg %%bx, %%bx"
			 : "=a"(ret) : "S"(OSA_OP_XGETTXID) : "memory");
   return ret;
}

static __inline__ void _xend() {
   magic_instruction(OSA_OP_XEND);
}

static __inline__ unsigned int _xbegin(unsigned short user_code) {
   unsigned int ret;
   __asm__ __volatile__("xchg %%bx, %%bx"
			:"=a"(ret) : "S"(OSA_OP_XBEGIN), "a"(user_code)
			: "memory", "cc");
   return ret; 
}

static __inline__ void _xretry(unsigned short code) {
   __asm__ __volatile__("xchg %%bx, %%bx"
         : : "S"(OSA_OP_XRESTART), "a"(code) : "memory");
}

#else /* !USE_MAGIC_TXOPCODES */

#define XBEGIN_STR      ".byte 0xf1\n\t"
#define XEND_STR        ".byte 0xd6\n\t"
#define XPUSH_STR       ".byte 0x0f\n\t.byte 0x24\n\t"
#define XPOP_STR        ".byte 0x0f\n\t.byte 0x25\n\t"
#define XRETRY_STR      ".byte 0x0f\n\t.byte 0x38\n\t"
#define XGETTXID_STR    ".byte 0x0f\n\t.byte 0x39\n\t"

static inline void _xpush() {
  __asm__ __volatile__(XPUSH_STR : : : "memory", "cc");
}

static __inline__ void _xpop() {
   __asm__ __volatile__(XPOP_STR : : : "memory");
}

static __inline__ unsigned int _xgettxid() {
   unsigned int ret;
   __asm__ __volatile__(XGETTXID_STR
         : "=a"(ret) : : "memory");
   return ret;
}

static __inline__ void _xend() {
   __asm__ __volatile__(XEND_STR : : : "memory");
}

static __inline__ unsigned int _xbegin(unsigned short user_code) {
   unsigned int ret;
   __asm__ __volatile__(XBEGIN_STR : "=a"(ret) : "a"(user_code): "memory");
   return ret;
}

static __inline__ void _xretry(unsigned short code) {
   __asm__ __volatile__(XRETRY_STR : : "a"(code) : "memory");
}


#endif /* USE_MAGIC_TXOPCODES */

#endif
