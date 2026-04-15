/*
 * isr.h — kernel critical section interface
 *
 * Pulls in the architecture-specific implementation. The build system adds
 * the appropriate arch directory to the include path, so "critical.h"
 * resolves to arch/arm/cortex-m4/critical.h (or whichever arch is active).
 *
 * Kernel code should include this header, not an arch path directly.
 */
#ifndef CREST_ISR_H
#define CREST_ISR_H

#include "critical.h"

#endif /* CREST_ISR_H */
