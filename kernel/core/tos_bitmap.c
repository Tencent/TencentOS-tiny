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

#include "tos_k.h"

__API__ k_err_t tos_bitmap_create_empty(k_bitmap_t *bitmap, k_bmtbl_t *bitmap_tbl, uint32_t bit_max)
{
    int i = 0;

    TOS_PTR_SANITY_CHECK(bitmap);

    bitmap->bitmap_tbl  = bitmap_tbl;
    bitmap->bit_max     = bit_max;
    bitmap->bit_ndx_max = K_BITMAP_TBL_SIZE(bit_max);

    for (i = 0; i < bitmap->bit_ndx_max; ++i) {
        /* all bits are 0 */
        bitmap_tbl[i] = 0;
    }
    TOS_OBJ_INIT(bitmap, KNL_OBJ_TYPE_BITMAP);

    return K_ERR_NONE;
}

__API__ k_err_t tos_bitmap_create_full(k_bitmap_t *bitmap, k_bmtbl_t *bitmap_tbl, uint32_t bit_max)
{
    int i = 0;

    TOS_PTR_SANITY_CHECK(bitmap);

    bitmap->bitmap_tbl  = bitmap_tbl;
    bitmap->bit_max     = bit_max;
    bitmap->bit_ndx_max = K_BITMAP_TBL_SIZE(bit_max);

    for (i = 0; i < bitmap->bit_ndx_max; ++i) {
        /* all bits are 1 */
        bitmap_tbl[i] = ~0;
    }
    TOS_OBJ_INIT(bitmap, KNL_OBJ_TYPE_BITMAP);

    return K_ERR_NONE;
}

__API__ k_err_t tos_bitmap_destroy(k_bitmap_t *bitmap)
{
    TOS_PTR_SANITY_CHECK(bitmap);
    TOS_OBJ_VERIFY(bitmap, KNL_OBJ_TYPE_BITMAP);

    TOS_OBJ_DEINIT(bitmap);
    return K_ERR_NONE;
}

__API__ k_err_t tos_bitmap_set(k_bitmap_t *bitmap, uint32_t bit)
{
    TOS_PTR_SANITY_CHECK(bitmap);
    TOS_OBJ_VERIFY(bitmap, KNL_OBJ_TYPE_BITMAP);

    if (bit >= bitmap->bit_max) {
        return K_ERR_BITMAP_EXCEED;
    }

    bitmap->bitmap_tbl[K_BITMAP_NDX(bit)] |= K_BITMAP_BIT(bit);

    return K_ERR_NONE;
}

__API__ k_err_t tos_bitmap_reset(k_bitmap_t *bitmap, uint32_t bit)
{
    TOS_PTR_SANITY_CHECK(bitmap);
    TOS_OBJ_VERIFY(bitmap, KNL_OBJ_TYPE_BITMAP);

    if (bit >= bitmap->bit_max) {
        return K_ERR_BITMAP_EXCEED;
    }

    bitmap->bitmap_tbl[K_BITMAP_NDX(bit)] &= ~K_BITMAP_BIT(bit);

    return K_ERR_NONE;
}

__API__ int tos_bitmap_is_set(k_bitmap_t *bitmap, uint32_t bit)
{
    TOS_PTR_SANITY_CHECK_RC(bitmap, K_FALSE);
    TOS_OBJ_VERIFY_RC(bitmap, KNL_OBJ_TYPE_BITMAP, K_FALSE);

    if (bit >= bitmap->bit_max) {
        return K_FALSE;
    }

    return (bitmap->bitmap_tbl[K_BITMAP_NDX(bit)] & K_BITMAP_BIT(bit)) ? K_TRUE : K_FALSE;
}

__API__ int tos_bitmap_is_reset(k_bitmap_t *bitmap, uint32_t bit)
{
    TOS_PTR_SANITY_CHECK_RC(bitmap, K_FALSE);
    TOS_OBJ_VERIFY_RC(bitmap, KNL_OBJ_TYPE_BITMAP, K_FALSE);

    if (bit >= bitmap->bit_max) {
        return K_FALSE;
    }

    return tos_bitmap_is_set(bitmap, bit) ? K_FALSE : K_TRUE;
}

__API__ int tos_bitmap_lsb(k_bitmap_t *bitmap)
{
    int lsb = 0, i = 0;
    k_bmtbl_t *bitmap_tbl;

    TOS_PTR_SANITY_CHECK_RC(bitmap, -1);
    TOS_OBJ_VERIFY_RC(bitmap, KNL_OBJ_TYPE_BITMAP, -1);

    bitmap_tbl = bitmap->bitmap_tbl;

    for (i = 0; i < bitmap->bit_ndx_max; ++i) {
        if (*bitmap_tbl == 0) {
            ++bitmap_tbl;
        }
        else{
            lsb = i * K_BITMAP_SLOT_SIZE;
            lsb += tos_cpu_clz(*bitmap_tbl);
            return lbs;
        }
    }
    
    // 如果没找到为什么不返回 -1(not_exist) 之类的值
    return bitmap->bit_max;
}

