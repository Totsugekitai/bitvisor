/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the University of Tsukuba nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * VMM core interface exported to the drivers
 * @file core.h
 * @author T. Shinagawa
 */
#ifndef _CORE_H
#define _CORE_H
#include <bits.h>
#include <common.h>
#include <constants.h>
#include <core/config.h>

/** I/O handler functions */
#include <io.h>

/** lock functions */
#include <core/spinlock.h>

/** memory functions */
#include <core/mm.h>
#define DEFINE_ALLOC_FUNC(type)			\
static struct type *alloc_##type()		\
{						\
	return alloc(sizeof(struct type));	\
}

#define DEFINE_ZALLOC_FUNC(type)			\
static struct type *zalloc_##type()			\
{							\
	void *p;					\
	p = alloc(sizeof(struct type));			\
	if (p)						\
		memset(p, 0, sizeof(struct type));	\
	return p;					\
}

#define DEFINE_CALLOC_FUNC(type)			\
static struct type *calloc_##type(int n)		\
{							\
	void *p;					\
	p = alloc(sizeof(struct type) * n);		\
	if (p)						\
		memset(p, 0, sizeof(struct type) * n);	\
	return p;					\
}

/** debug functions */
#include <core/panic.h>
#include <core/printf.h>

/** init functions */
#include <core/initfunc.h>
/* a function only used at initialization */
#define __initcode__ // __attribute__((__section__(".initcode")))
/* a function only used at initialization */
#define __initdata__ // __attribute__((__section__(".initdata")))

/* a function automatically called at initialization */ 
#define CORE_INIT(func)		INITFUNC ("driver0", func)
#define CRYPTO_INIT(func)	INITFUNC ("driver1", func)
#define DRIVER_INIT(func)	INITFUNC ("driver2", func)
#define PCI_DRIVER_INIT(func)	INITFUNC ("driver3", func)
#define DEBUG_DRIVER_INIT(func)	INITFUNC ("driver9", func)

#if defined (__i386__) || defined (__x86_64__)

static inline void
asm_rep_and_nop (void)
{
	asm volatile ("rep ; nop");
}

static inline void
asm_pause (void)
{
	asm volatile ("pause");
}

static inline void
asm_store_barrier (void)
{
	asm volatile ("sfence" : : : "memory");
}

#else
#error "Unsupported architecture"
#endif

#define cpu_relax            asm_pause

#endif
