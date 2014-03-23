#ifndef SIMICS_H
#define SIMICS_H

#include <string.h>
#include <stdio.h>

#define OSA_BREAKSIM      5
#define OSA_CLEAR_STAT    2
#define OSA_OUTPUT_STAT   3
#define OSA_PRINT_STR_VAL 1

#define SYNCCHAR_LOAD_MAP     17

#define OSA_LOG_THREAD_START  18
#define OSA_LOG_THREAD_STOP   19

#define OSA_USER_OVERFLOW_BEGIN  1400
#define OSA_USER_OVERFLOW_END    1401
#define OSA_USER_ABORTALL        1402

static __inline__ void magic_instruction(unsigned int code) {
   __asm__ __volatile__ ("movl %0, %%esi; xchg %%bx, %%bx"
         : : "g" (code) : "esi");
}


static __inline__ void osa_print(char * str, unsigned int num) {
   __asm__ __volatile__ ("xchg %%bx, %%bx " \
         : /*no output*/				  \
         : "b"(str), "c"(num), "S"(OSA_PRINT_STR_VAL) );
}

#define OSA_PRINT_BUF_LEN 1024
extern char osa_print_buf[];
extern char osa_line_buf[];

static inline void print_buf(char *cur, int len) {
   char *nextnl = NULL;
   char *end = &cur[len];
   *end = '\n';
   while(cur < end) {
      nextnl = strchr(cur, '\n');
      memcpy(osa_line_buf + 6, cur, nextnl - cur);
      osa_line_buf[6 + (nextnl - cur)] = 0;
      osa_print(osa_line_buf, 0xdeadcafe);
      if(*nextnl == 0)
         break;
      cur = nextnl + 1;
   } 
}

#define printf(...) \
   ({ int ret = snprintf(osa_print_buf, OSA_PRINT_BUF_LEN, __VA_ARGS__); \
      printf(osa_print_buf); \
      print_buf(osa_print_buf, ret); \
      ret; })

#define puts(str) \
   ({ strcpy(osa_line_buf + 6, str); OSA_PRINT(osa_line_buf, 0xdeadcafe); \
      puts(str); })


static __inline__ void load_syncchar_map(char *filename){
#ifdef LOCK
  size_t len = strlen(filename) + 1;
  printf("Loading %s\n", filename);
  __asm__ __volatile__("movl %0, %%esi; movl %1, %%ebx; movl %2, %%ecx; xchg %%bx, %%bx" \
		       :						\
		       : "g"(SYNCCHAR_LOAD_MAP), "g"(filename), "g"(len) \
		       : "memory", "esi", "ebx", "ecx");
#endif
}

#define LOG_THREAD_START() magic_instruction(OSA_LOG_THREAD_START)
#define LOG_THREAD_STOP() magic_instruction(OSA_LOG_THREAD_STOP)

#endif /* SIMICS_H */
