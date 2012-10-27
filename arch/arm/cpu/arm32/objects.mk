#/**
# Copyright (c) 2011 Pranav Sawargaonkar.
# All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
# @file objects.mk
# @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
# @author Anup Patel (anup@brainfault.org)
# @brief list of ARM32 cpu objects.
# */

# This selects which instruction set is used.
# Note that GCC does not numerically define an architecture version
# macro, but instead defines a whole series of macros which makes
# testing for a specific architecture or later rather impossible.
arch-$(CONFIG_ARMV7A) += -mno-thumb-interwork -march=armv7-a
arch-$(CONFIG_ARMV5) += -mno-thumb-interwork -march=armv5te

# This selects how we optimise for the processor.
tune-$(CONFIG_CPU_CORTEX_A8)  += -mcpu=cortex-a8
tune-$(CONFIG_CPU_OMAP3)      += -mcpu=cortex-a8
tune-$(CONFIG_CPU_CORTEX_A9)  += -mcpu=cortex-a9
tune-$(CONFIG_CPU_CORTEX_A15) += -mcpu=cortex-a15

# Need -Uarm for gcc < 3.x
cpu-cppflags+=-DCPU_TEXT_START=0xFF000000
cpu-cflags += -msoft-float -marm -Uarm $(arch-y) $(tune-y)
ifeq ($(CONFIG_ARM32_STACKTRACE), y)
cpu-cflags += -fno-omit-frame-pointer -mapcs -mno-sched-prolog
endif
cpu-asflags += -marm $(arch-y) $(tune-y)
cpu-ldflags += -msoft-float

cpu-objs-$(CONFIG_ARMV5)+= cpu_entry_v5.o
cpu-objs-$(CONFIG_ARMV5)+= cpu_mmu_v5.o
cpu-objs-$(CONFIG_ARMV5)+= cpu_cache_v5.o
cpu-objs-$(CONFIG_ARMV5)+= cpu_atomic_v5.o

cpu-objs-$(CONFIG_ARMV7A)+= cpu_entry_v7.o
cpu-objs-$(CONFIG_ARMV7A)+= cpu_mmu_v7.o
cpu-objs-$(CONFIG_ARMV7A)+= cpu_cache_v7.o
cpu-objs-$(CONFIG_ARMV7A)+= cpu_atomic_v7.o

cpu-objs-y+= cpu_init.o
cpu-objs-y+= cpu_delay.o
cpu-objs-y+= cpu_elf.o
cpu-objs-$(CONFIG_ARM32_STACKTRACE)+= cpu_stacktrace.o
ifdef CONFIG_ARMV7A
cpu-objs-$(CONFIG_SMP)+= cpu_smp.o
endif
cpu-objs-$(CONFIG_SMP)+= cpu_locks.o
cpu-objs-y+= cpu_interrupts.o
cpu-objs-y+= cpu_vcpu_helper.o
cpu-objs-y+= cpu_vcpu_coproc.o
cpu-objs-y+= cpu_vcpu_cp15.o
cpu-objs-y+= cpu_vcpu_mem.o
cpu-objs-y+= cpu_vcpu_irq.o
cpu-objs-y+= cpu_vcpu_hypercall_arm.o
cpu-objs-y+= cpu_vcpu_hypercall_thumb.o

