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

__STATIC_INLINE__ void ring_q_item_copy_to(k_ring_q_t *ring_q, void *item_out, size_t *item_size)
{
    memcpy(item_out, RING_HEAD_ITEM(ring_q), ring_q->item_size);
    if (item_size) {
        *item_size = ring_q->item_size;
    }
}

__STATIC_INLINE__ void ring_q_item_copy_from(k_ring_q_t *ring_q, void *item_in)
{
    memcpy(RING_TAIL_ITEM(ring_q), item_in, ring_q->item_size);
}

__STATIC_INLINE__ void ring_q_item_increase(k_ring_q_t *ring_q)
{
    ring_q->tail = RING_NEXT(ring_q, ring_q->tail);
    ++ring_q->total;
}

__STATIC_INLINE__ void ring_q_item_decrease(k_ring_q_t *ring_q)
{
    ring_q->head = RING_NEXT(ring_q, ring_q->head);
    --ring_q->total;
}

 __API__ k_err_t tos_ring_q_create(k_ring_q_t *ring_q, void *pool, size_t item_cnt, size_t item_size)
{
    TOS_PTR_SANITY_CHECK(ring_q);
    TOS_PTR_SANITY_CHECK(pool);

    ring_q->head        = 0u;
    ring_q->tail        = 0u;
    ring_q->total       = 0;

    ring_q->pool        = (uint8_t *)pool;
    ring_q->item_size   = item_size;
    ring_q->item_cnt    = item_cnt;

    TOS_OBJ_INIT(ring_q, KNL_OBJ_TYPE_RING_QUEUE);
    knl_object_alloc_set_static(&ring_q->knl_obj);

    return K_ERR_NONE;
}

__API__ k_err_t tos_ring_q_destroy(k_ring_q_t *ring_q)
{
    TOS_PTR_SANITY_CHECK(ring_q);
    TOS_OBJ_VERIFY(ring_q, KNL_OBJ_TYPE_RING_QUEUE);

    if (!knl_object_alloc_is_static(&ring_q->knl_obj)) {
        return K_ERR_OBJ_INVALID_ALLOC_TYPE;
    }

    ring_q->head        = 0u;
    ring_q->tail        = 0u;
    ring_q->total       = 0;

    ring_q->pool        = K_NULL;
    ring_q->item_size   = 0u;
    ring_q->item_cnt    = 0u;

    TOS_OBJ_DEINIT(ring_q);
    knl_object_alloc_reset(&ring_q->knl_obj);

    return K_ERR_NONE;
}

 __API__ k_err_t tos_ring_q_create_dyn(k_ring_q_t *ring_q, size_t item_cnt, size_t item_size)
{
    void *pool;

    TOS_PTR_SANITY_CHECK(ring_q);

    pool = tos_mmheap_alloc(item_cnt * item_size);
    if (!pool) {
        return K_ERR_OUT_OF_MEMORY;
    }

    ring_q->head        = 0u;
    ring_q->tail        = 0u;
    ring_q->total       = 0;

    ring_q->pool        = (uint8_t *)pool;
    ring_q->item_size   = item_size;
    ring_q->item_cnt    = item_cnt;

    TOS_OBJ_INIT(ring_q, KNL_OBJ_TYPE_RING_QUEUE);
    knl_object_alloc_set_dynamic(&ring_q->knl_obj);

    return K_ERR_NONE;
}

__API__ k_err_t tos_ring_q_destroy_dyn(k_ring_q_t *ring_q)
{
    TOS_PTR_SANITY_CHECK(ring_q);
    TOS_OBJ_VERIFY(ring_q, KNL_OBJ_TYPE_RING_QUEUE);

    if (!knl_object_alloc_is_dynamic(&ring_q->knl_obj)) {
        return K_ERR_OBJ_INVALID_ALLOC_TYPE;
    }

    tos_mmheap_free(ring_q->pool);

    ring_q->head        = 0u;
    ring_q->tail        = 0u;
    ring_q->total       = 0;

    ring_q->pool        = K_NULL;
    ring_q->item_size   = 0u;
    ring_q->item_cnt    = 0u;

    TOS_OBJ_DEINIT(ring_q);
    knl_object_alloc_reset(&ring_q->knl_obj);

    return K_ERR_NONE;
}

__API__ k_err_t tos_ring_q_enqueue(k_ring_q_t *ring_q, void *item, size_t item_size)
{
    TOS_CPU_CPSR_ALLOC();

    TOS_PTR_SANITY_CHECK(ring_q);
    TOS_PTR_SANITY_CHECK(item);
    TOS_OBJ_VERIFY(ring_q, KNL_OBJ_TYPE_RING_QUEUE);

    if (item_size != ring_q->item_size) {
        return K_ERR_RING_Q_ITEM_SIZE_NOT_MATCH;
    }

    TOS_CPU_INT_DISABLE();

    if (tos_ring_q_is_full(ring_q)) {
        TOS_CPU_INT_ENABLE();
        return K_ERR_RING_Q_FULL;
    }

    ring_q_item_copy_from(ring_q, item);
    ring_q_item_increase(ring_q);

    TOS_CPU_INT_ENABLE();
    return K_ERR_NONE;
}

__API__ k_err_t tos_ring_q_dequeue(k_ring_q_t *ring_q, void *item, size_t *item_size)
{
    TOS_CPU_CPSR_ALLOC();

    TOS_PTR_SANITY_CHECK(ring_q);
    TOS_PTR_SANITY_CHECK(item);
    TOS_OBJ_VERIFY(ring_q, KNL_OBJ_TYPE_RING_QUEUE);

    TOS_CPU_INT_DISABLE();

    // tos_ring_q_is_empty 内部使用了 TOS_CPU_INT_ENABLE()
    // 如果中断的开闭不是递归锁将导致同步问题
    // 其他地方应该也有类似的问题
    if (tos_ring_q_is_empty(ring_q)) {
        TOS_CPU_INT_ENABLE();
        return K_ERR_RING_Q_EMPTY;
    }

    ring_q_item_copy_to(ring_q, item, item_size);
    ring_q_item_decrease(ring_q);

    TOS_CPU_INT_ENABLE();

    return K_ERR_NONE;
}

__API__ k_err_t tos_ring_q_flush(k_ring_q_t *ring_q)
{
    TOS_CPU_CPSR_ALLOC();

    TOS_PTR_SANITY_CHECK(ring_q);
    TOS_OBJ_VERIFY(ring_q, KNL_OBJ_TYPE_RING_QUEUE);

    TOS_CPU_INT_DISABLE();

    ring_q->head    = 0u;
    ring_q->tail    = 0u;
    ring_q->total   = 0;

    TOS_CPU_INT_ENABLE();

    return K_ERR_NONE;
}

__API__ int tos_ring_q_is_empty(k_ring_q_t *ring_q)
{
    TOS_CPU_CPSR_ALLOC();
    int is_empty = K_FALSE;

    TOS_PTR_SANITY_CHECK_RC(ring_q, K_FALSE);
    TOS_OBJ_VERIFY_RC(ring_q, KNL_OBJ_TYPE_RING_QUEUE, K_FALSE);

    TOS_CPU_INT_DISABLE();
    is_empty = (ring_q->total == 0 ? K_TRUE : K_FALSE);
    TOS_CPU_INT_ENABLE();

    return is_empty;
}

__API__ int tos_ring_q_is_full(k_ring_q_t *ring_q)
{
    TOS_CPU_CPSR_ALLOC();
    int is_full = K_FALSE;

    TOS_PTR_SANITY_CHECK_RC(ring_q, K_FALSE);
    TOS_OBJ_VERIFY_RC(ring_q, KNL_OBJ_TYPE_RING_QUEUE, K_FALSE);

    TOS_CPU_INT_DISABLE();
    is_full = (ring_q->total == ring_q->item_cnt ? K_TRUE : K_FALSE);
    TOS_CPU_INT_ENABLE();

    return is_full;
}

