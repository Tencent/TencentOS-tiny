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

#ifndef _PORT_CONFIG_H_
#define _PORT_CONFIG_H_

#define TOS_CFG_CPU_ADDR_SIZE                   CPU_WORD_SIZE_32
#define TOS_CFG_CPU_DATA_SIZE                   CPU_WORD_SIZE_32
#define TOS_CFG_CPU_STK_GROWTH                  CPU_STK_GROWTH_DESCENDING
#define TOS_CFG_CPU_HRTIMER_EN                  0u
#define TOS_CFG_CPU_LEAD_ZEROS_ASM_PRESENT      0u
#define TOS_CFG_CPU_BYTE_ORDER                  CPU_BYTE_ORDER_LITTLE_ENDIAN

#if __riscv_xlen == 64
# define LWU      lwu
#else
# define LWU      lw
#endif

#endif /* _PORT_CONFIG_H_ */

