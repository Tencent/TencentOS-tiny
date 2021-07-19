/*----------------------------------------------------------------------------
 * Tencent is pleased to support the open source community by making TencentOS
 * available.
 *
 * Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
 * If you have downloaded a copy of the TencentOS binary from Tencent, please
 * note that the TencentOS binary is licensed under the BSD 3-Clause License.
 *
 * If you have downloaded a copy of the TencentOS source code from Tencent,
 * please note that TencentOS source code is licensed under the BSD 3-Clause
 * License, except for the third-party components listed below which are
 * subject to different license terms. Your integration of TencentOS into your
 * own projects may require compliance with the BSD 3-Clause License, as well
 * as the other licenses applicable to the third-party components included
 * within TencentOS.
 *---------------------------------------------------------------------------*/

#include "riscv_port.h"

.global port_int_disable
.global port_int_enable

.global port_cpsr_save
.global port_cpsr_restore

.global port_systick_resume
.global port_systick_suspend
.global port_systick_pending_reset

.global port_sched_start
.global port_context_switch
.global rv32_exception_entry

.extern k_curr_task
.extern k_next_task

.equ    MSTATUS_MIE,        0x00000008
.equ    MSTATUS_MPP,        0x00001800

.equ    MIE_MTIE,           (1 << 7)        // machine mode interrupt enable

.equ    MIP_MTIP,           (1 << 7)        // machine mode timer interrupt pending

.text
.align 2

.type port_int_disable, %function
port_int_disable:
    csrci   mstatus, MSTATUS_MIE
    ret

.type port_int_enable, %function
port_int_enable:
    csrsi   mstatus, MSTATUS_MIE
    ret

.type port_cpsr_save, %function
port_cpsr_save:
    csrrci  a0, mstatus, MSTATUS_MIE
    ret

.type port_cpsr_restore, %function
port_cpsr_restore:
    csrw    mstatus, a0
    ret

.type port_systick_resume, %function
port_systick_resume:
    li t0, MIE_MTIE
    csrs mie, t0
    ret

.type port_systick_suspend, %function
port_systick_suspend:
    li t0, MIE_MTIE
    csrc mie, t0
    ret

.type port_systick_pending_reset, %function
port_systick_pending_reset:
    li t0, MIP_MTIP
    csrc mip, t0
    ret


#define __reg_mepc_OFFSET       0x00
#define __reg_mstatus_OFFSET    0x04
#define __reg_x1_OFFSET         0x08
#define __reg_x3_OFFSET         0x0C
#define __reg_x4_OFFSET         0x10
#define __reg_x5_OFFSET         0x14
#define __reg_x6_OFFSET         0x18
#define __reg_x7_OFFSET         0x1C
#define __reg_x8_OFFSET         0x20
#define __reg_x9_OFFSET         0x24
#define __reg_x10_OFFSET        0x28
#define __reg_x11_OFFSET        0x2C
#define __reg_x12_OFFSET        0x30
#define __reg_x13_OFFSET        0x34
#define __reg_x14_OFFSET        0x38
#define __reg_x15_OFFSET        0x3C
#define __reg_x16_OFFSET        0x40
#define __reg_x17_OFFSET        0x44
#define __reg_x18_OFFSET        0x48
#define __reg_x19_OFFSET        0x4C
#define __reg_x20_OFFSET        0x50
#define __reg_x21_OFFSET        0x54
#define __reg_x22_OFFSET        0x58
#define __reg_x23_OFFSET        0x5C
#define __reg_x24_OFFSET        0x60
#define __reg_x25_OFFSET        0x64
#define __reg_x26_OFFSET        0x68
#define __reg_x27_OFFSET        0x6C
#define __reg_x28_OFFSET        0x70
#define __reg_x29_OFFSET        0x74
#define __reg_x30_OFFSET        0x78
#define __reg_x31_OFFSET        0x7C

#define __reg_mepc__OFFSET      __reg_mepc_OFFSET
#define __reg_mstatus__OFFSET   __reg_mstatus_OFFSET
#define __reg_ra__OFFSET        __reg_x1_OFFSET
#define __reg_gp__OFFSET        __reg_x3_OFFSET
#define __reg_tp__OFFSET        __reg_x4_OFFSET
#define __reg_t0__OFFSET        __reg_x5_OFFSET
#define __reg_t1__OFFSET        __reg_x6_OFFSET
#define __reg_t2__OFFSET        __reg_x7_OFFSET
#define __reg_s0__OFFSET        __reg_x8_OFFSET
#define __reg_fp__OFFSET        __reg_x8_OFFSET
#define __reg_s1__OFFSET        __reg_x9_OFFSET
#define __reg_a0__OFFSET        __reg_x10_OFFSET
#define __reg_a1__OFFSET        __reg_x11_OFFSET
#define __reg_a2__OFFSET        __reg_x12_OFFSET
#define __reg_a3__OFFSET        __reg_x13_OFFSET
#define __reg_a4__OFFSET        __reg_x14_OFFSET
#define __reg_a5__OFFSET        __reg_x15_OFFSET
#define __reg_a6__OFFSET        __reg_x16_OFFSET
#define __reg_a7__OFFSET        __reg_x17_OFFSET
#define __reg_s2__OFFSET        __reg_x18_OFFSET
#define __reg_s3__OFFSET        __reg_x19_OFFSET
#define __reg_s4__OFFSET        __reg_x20_OFFSET
#define __reg_s5__OFFSET        __reg_x21_OFFSET
#define __reg_s6__OFFSET        __reg_x22_OFFSET
#define __reg_s7__OFFSET        __reg_x23_OFFSET
#define __reg_s8__OFFSET        __reg_x24_OFFSET
#define __reg_s9__OFFSET        __reg_x25_OFFSET
#define __reg_s10__OFFSET       __reg_x26_OFFSET
#define __reg_s11__OFFSET       __reg_x27_OFFSET
#define __reg_t3__OFFSET        __reg_x28_OFFSET
#define __reg_t4__OFFSET        __reg_x29_OFFSET
#define __reg_t5__OFFSET        __reg_x30_OFFSET
#define __reg_t6__OFFSET        __reg_x31_OFFSET


.align 2
.type port_sched_start, %function
port_sched_start:
    // enable timer interrupt
    li      t0, MIE_MTIE
    csrs    mie, t0

    // load sp from k_curr_task->sp
    LWU      t0, k_curr_task
    LWU      sp, (t0)                // sp = k_curr_task->sp

    j       restore_context


.align 2
.type port_context_switch, %function
port_context_switch:
    addi   sp, sp, -128
    sw x1, __reg_x1_OFFSET(sp)
    sw x3, __reg_x3_OFFSET(sp)
    sw x4, __reg_x4_OFFSET(sp)
    sw x5, __reg_x5_OFFSET(sp)
    sw x6, __reg_x6_OFFSET(sp)
    sw x7, __reg_x7_OFFSET(sp)
    sw x8, __reg_x8_OFFSET(sp)
    sw x9, __reg_x9_OFFSET(sp)
    sw x10, __reg_x10_OFFSET(sp)
    sw x11, __reg_x11_OFFSET(sp)
    sw x12, __reg_x12_OFFSET(sp)
    sw x13, __reg_x13_OFFSET(sp)
    sw x14, __reg_x14_OFFSET(sp)
    sw x15, __reg_x15_OFFSET(sp)
    sw x16, __reg_x16_OFFSET(sp)
    sw x17, __reg_x17_OFFSET(sp)
    sw x18, __reg_x18_OFFSET(sp)
    sw x19, __reg_x19_OFFSET(sp)
    sw x20, __reg_x20_OFFSET(sp)
    sw x21, __reg_x21_OFFSET(sp)
    sw x22, __reg_x22_OFFSET(sp)
    sw x23, __reg_x23_OFFSET(sp)
    sw x24, __reg_x24_OFFSET(sp)
    sw x25, __reg_x25_OFFSET(sp)
    sw x26, __reg_x26_OFFSET(sp)
    sw x27, __reg_x27_OFFSET(sp)
    sw x28, __reg_x28_OFFSET(sp)
    sw x29, __reg_x29_OFFSET(sp)
    sw x30, __reg_x30_OFFSET(sp)
    sw x31, __reg_x31_OFFSET(sp)

    sw     ra,  __reg_mepc_OFFSET(sp)

    csrr   t0,  mstatus
    li     t1,  MSTATUS_MPP
    or     t0,  t0, t1
    sw     t0,  __reg_mstatus_OFFSET(sp)


switch_task:
    la      t0, k_curr_task         // t0 = &k_curr_task
    la      t1, k_next_task         // t1 = &k_next_task

    // save sp to k_curr_task.sp
    LWU      t2, (t0)
    sw      sp, (t2)

    // switch task
    // k_curr_task = k_next_task
    LWU      t1, (t1)                // t1 = k_next_task
    sw      t1, (t0)

    // load new task sp
    LWU      sp, (t1)

restore_context:
    // restore context
    LWU      t0,   __reg_mepc_OFFSET(sp)
    csrw    mepc, t0

    LWU      t0,   __reg_mstatus_OFFSET(sp)
    csrw    mstatus, t0

    LWU x1, __reg_x1_OFFSET(sp)
    LWU x3, __reg_x3_OFFSET(sp)
    LWU x4, __reg_x4_OFFSET(sp)
    LWU x5, __reg_x5_OFFSET(sp)
    LWU x6, __reg_x6_OFFSET(sp)
    LWU x7, __reg_x7_OFFSET(sp)
    LWU x8, __reg_x8_OFFSET(sp)
    LWU x9, __reg_x9_OFFSET(sp)
    LWU x10, __reg_x10_OFFSET(sp)
    LWU x11, __reg_x11_OFFSET(sp)
    LWU x12, __reg_x12_OFFSET(sp)
    LWU x13, __reg_x13_OFFSET(sp)
    LWU x14, __reg_x14_OFFSET(sp)
    LWU x15, __reg_x15_OFFSET(sp)
    LWU x16, __reg_x16_OFFSET(sp)
    LWU x17, __reg_x17_OFFSET(sp)
    LWU x18, __reg_x18_OFFSET(sp)
    LWU x19, __reg_x19_OFFSET(sp)
    LWU x20, __reg_x20_OFFSET(sp)
    LWU x21, __reg_x21_OFFSET(sp)
    LWU x22, __reg_x22_OFFSET(sp)
    LWU x23, __reg_x23_OFFSET(sp)
    LWU x24, __reg_x24_OFFSET(sp)
    LWU x25, __reg_x25_OFFSET(sp)
    LWU x26, __reg_x26_OFFSET(sp)
    LWU x27, __reg_x27_OFFSET(sp)
    LWU x28, __reg_x28_OFFSET(sp)
    LWU x29, __reg_x29_OFFSET(sp)
    LWU x30, __reg_x30_OFFSET(sp)
    LWU x31, __reg_x31_OFFSET(sp)
    addi    sp, sp, 128

    mret


.align 6
.global rv32_exception_entry
rv32_exception_entry:
    addi   sp,  sp, -128
    sw ra, __reg_ra__OFFSET(sp)
    sw gp, __reg_gp__OFFSET(sp)
    sw tp, __reg_tp__OFFSET(sp)
    sw t0, __reg_t0__OFFSET(sp)
    sw t1, __reg_t1__OFFSET(sp)
    sw t2, __reg_t2__OFFSET(sp)
    sw t3, __reg_t3__OFFSET(sp)
    sw t4, __reg_t4__OFFSET(sp)
    sw t5, __reg_t5__OFFSET(sp)
    sw t6, __reg_t6__OFFSET(sp)
    sw a0, __reg_a0__OFFSET(sp)
    sw a1, __reg_a1__OFFSET(sp)
    sw a2, __reg_a2__OFFSET(sp)
    sw a3, __reg_a3__OFFSET(sp)
    sw a4, __reg_a4__OFFSET(sp)
    sw a5, __reg_a5__OFFSET(sp)
    sw a6, __reg_a6__OFFSET(sp)
    sw a7, __reg_a7__OFFSET(sp)

    csrr    t0,  mepc
    sw      t0,  __reg_mepc__OFFSET(sp)

    csrr    t0,  mstatus
    sw      t0,  __reg_mstatus__OFFSET(sp)

    mv      t0, sp
    // switch to irq stack
    LWU      sp, k_irq_stk_top
    // save task stack pointer
    sw      t0, (sp)

    // get irq num and call irq handler
    li      t0,  MCAUSE_EXP_CODE_MASK
    csrr    a0,  mcause
    and     a0,  a0, t0
    call    cpu_irq_entry

    // switch back to task stack
    LWU      sp, (sp)

    LWU      t0,  k_curr_task
    LWU      t1,  k_next_task

    // unlikely
    bne     t0,  t1, irq_task_switch

irq_restore:
    LWU      t0,   __reg_mepc_OFFSET(sp)
    csrw    mepc, t0

    LWU      t0,   __reg_mstatus_OFFSET(sp)
    csrw    mstatus, t0

    LWU ra, __reg_ra__OFFSET(sp)
    LWU gp, __reg_gp__OFFSET(sp)
    LWU tp, __reg_tp__OFFSET(sp)
    LWU t0, __reg_t0__OFFSET(sp)
    LWU t1, __reg_t1__OFFSET(sp)
    LWU t2, __reg_t2__OFFSET(sp)
    LWU t3, __reg_t3__OFFSET(sp)
    LWU t4, __reg_t4__OFFSET(sp)
    LWU t5, __reg_t5__OFFSET(sp)
    LWU t6, __reg_t6__OFFSET(sp)
    LWU a0, __reg_a0__OFFSET(sp)
    LWU a1, __reg_a1__OFFSET(sp)
    LWU a2, __reg_a2__OFFSET(sp)
    LWU a3, __reg_a3__OFFSET(sp)
    LWU a4, __reg_a4__OFFSET(sp)
    LWU a5, __reg_a5__OFFSET(sp)
    LWU a6, __reg_a6__OFFSET(sp)
    LWU a7, __reg_a7__OFFSET(sp)
    addi    sp, sp, 128

    mret

irq_task_switch:
    sw s0, __reg_s0__OFFSET(sp)
    sw s1, __reg_s1__OFFSET(sp)
    sw s2, __reg_s2__OFFSET(sp)
    sw s3, __reg_s3__OFFSET(sp)
    sw s4, __reg_s4__OFFSET(sp)
    sw s5, __reg_s5__OFFSET(sp)
    sw s6, __reg_s6__OFFSET(sp)
    sw s7, __reg_s7__OFFSET(sp)
    sw s8, __reg_s8__OFFSET(sp)
    sw s9, __reg_s9__OFFSET(sp)
    sw s10, __reg_s10__OFFSET(sp)
    sw s11, __reg_s11__OFFSET(sp)

    j switch_task

