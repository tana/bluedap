#pragma once

#define __STATIC_INLINE static inline
#define __STATIC_FORCEINLINE static inline

#define __WEAK __attribute__((weak))

#define __ASM asm

#define __NOP() asm volatile ("nop")