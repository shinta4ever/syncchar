#ifndef SPINLOCK_H
#define SPINLOCK_H

typedef struct {
  volatile unsigned int slock;
} spinlock_t;

static __inline__ void spin_lock(spinlock_t *lock){

  __asm__ __volatile__(	"\n1:\t" \
			"lock ; decb %0\n\t"	\
			"jns 3f\n"		\
			"2:\t"			\
			"rep;nop\n\t"		\
			"cmpb $0,%0\n\t"	\
			"jle 2b\n\t"		\
			"jmp 1b\n"		\
			"3:\n\t"
			: "=m"(lock->slock) : : "memory");
}

static __inline__ void spin_unlock(spinlock_t *lock){

  __asm__ __volatile__( "movb $1,%0"
			: "=m" (lock->slock) : : "memory" );
}

extern spinlock_t globalLock;

#endif /* SPINLOCK_H */
