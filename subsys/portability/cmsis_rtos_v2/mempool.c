/*
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/portability/cmsis_types.h>
#include <string.h>
#include "wrapper.h"

#define TIME_OUT_TICKS 10

K_MEM_SLAB_DEFINE(cv2_mem_slab, sizeof(struct cmsis_rtos_mempool_cb),
		  CONFIG_CMSIS_V2_MEM_SLAB_MAX_COUNT, 4);

static const osMemoryPoolAttr_t init_mslab_attrs = {
	.name = "ZephyrMemPool",
	.attr_bits = 0,
	.cb_mem = NULL,
	.cb_size = 0,
	.mp_mem = NULL,
	.mp_size = 0,
};

/**
 * @brief Create and Initialize a memory pool.
 */
osMemoryPoolId_t osMemoryPoolNew(uint32_t block_count, uint32_t block_size,
				 const osMemoryPoolAttr_t *attr)
{
	struct cmsis_rtos_mempool_cb *mslab;

	BUILD_ASSERT(K_HEAP_MEM_POOL_SIZE >= CONFIG_CMSIS_V2_MEM_SLAB_MAX_DYNAMIC_SIZE,
		     "heap must be configured to be at least the max dynamic size");

	if (k_is_in_isr()) {
		return NULL;
	}

	if ((attr != NULL) && (attr->mp_size < block_count * block_size)) {
		return NULL;
	}

	if (attr == NULL) {
		attr = &init_mslab_attrs;
	}

	if (attr->cb_mem != NULL) {
		__ASSERT(attr->cb_size == sizeof(struct cmsis_rtos_mempool_cb),
			 "Invalid cb_size\n");
		mslab = (struct cmsis_rtos_mempool_cb *)attr->cb_mem;
	} else if (k_mem_slab_alloc(&cv2_mem_slab, (void **)&mslab, K_MSEC(100)) != 0) {
		return NULL;
	}
	(void)memset(mslab, 0, sizeof(struct cmsis_rtos_mempool_cb));
	mslab->is_cb_dynamic_allocation = attr->cb_mem == NULL;

	if (attr->mp_mem == NULL) {
		__ASSERT((block_count * block_size) <= CONFIG_CMSIS_V2_MEM_SLAB_MAX_DYNAMIC_SIZE,
			 "memory slab/pool size exceeds dynamic maximum");

		mslab->pool = k_calloc(block_count, block_size);
		if (mslab->pool == NULL) {
			if (mslab->is_cb_dynamic_allocation) {
				k_mem_slab_free(&cv2_mem_slab, (void *)mslab);
			}
			return NULL;
		}
		mslab->is_dynamic_allocation = TRUE;
	} else {
		mslab->pool = attr->mp_mem;
		mslab->is_dynamic_allocation = FALSE;
	}

	int rc = k_mem_slab_init(&mslab->z_mslab, mslab->pool, block_size, block_count);
	if (rc != 0) {
		if (mslab->is_cb_dynamic_allocation) {
			k_mem_slab_free(&cv2_mem_slab, (void *)mslab);
		}
		if (attr->mp_mem == NULL) {
			k_free(mslab->pool);
		}
		return NULL;
	}

	mslab->name = (attr->name == NULL) ? init_mslab_attrs.name : attr->name;

	return (osMemoryPoolId_t)mslab;
}

/**
 * @brief Allocate a memory block from a memory pool.
 */
void *osMemoryPoolAlloc(osMemoryPoolId_t mp_id, uint32_t timeout)
{
	struct cmsis_rtos_mempool_cb *mslab = (struct cmsis_rtos_mempool_cb *)mp_id;
	int retval;
	void *ptr;

	if (mslab == NULL) {
		return NULL;
	}

	/* Can be called from ISRs only if timeout is set to 0 */
	if (timeout > 0 && k_is_in_isr()) {
		return NULL;
	}

	if (timeout == 0U) {
		retval = k_mem_slab_alloc((struct k_mem_slab *)(&mslab->z_mslab), (void **)&ptr,
					  K_NO_WAIT);
	} else if (timeout == osWaitForever) {
		retval = k_mem_slab_alloc((struct k_mem_slab *)(&mslab->z_mslab), (void **)&ptr,
					  K_FOREVER);
	} else {
		retval = k_mem_slab_alloc((struct k_mem_slab *)(&mslab->z_mslab), (void **)&ptr,
					  K_TICKS(timeout));
	}

	if (retval == 0) {
		return ptr;
	} else {
		return NULL;
	}
}

/**
 * @brief Return an allocated memory block back to a specific memory pool.
 */
osStatus_t osMemoryPoolFree(osMemoryPoolId_t mp_id, void *block)
{
	struct cmsis_rtos_mempool_cb *mslab = (struct cmsis_rtos_mempool_cb *)mp_id;

	if (mslab == NULL) {
		return osErrorParameter;
	}

	/* Note: Below error code is not supported.
	 *       osErrorResource: the memory pool specified by parameter mp_id
	 *       is in an invalid memory pool state.
	 */
	if (mslab->is_cb_dynamic_allocation) {
		k_mem_slab_free((struct k_mem_slab *)(&mslab->z_mslab), (void *)block);
	}
	return osOK;
}

/**
 * @brief Get name of a Memory Pool object.
 * This function may be called from Interrupt Service Routines.
 */
const char *osMemoryPoolGetName(osMemoryPoolId_t mp_id)
{
	struct cmsis_rtos_mempool_cb *mslab = (struct cmsis_rtos_mempool_cb *)mp_id;

	if (mslab == NULL) {
		return NULL;
	}
	return mslab->name;
}

/**
 * @brief Get maximum number of memory blocks in a Memory Pool.
 */
uint32_t osMemoryPoolGetCapacity(osMemoryPoolId_t mp_id)
{
	struct cmsis_rtos_mempool_cb *mslab = (struct cmsis_rtos_mempool_cb *)mp_id;

	if (mslab == NULL) {
		return 0;
	} else {
		return mslab->z_mslab.info.num_blocks;
	}
}

/**
 * @brief Get memory block size in a Memory Pool.
 */
uint32_t osMemoryPoolGetBlockSize(osMemoryPoolId_t mp_id)
{
	struct cmsis_rtos_mempool_cb *mslab = (struct cmsis_rtos_mempool_cb *)mp_id;

	if (mslab == NULL) {
		return 0;
	} else {
		return mslab->z_mslab.info.block_size;
	}
}

/**
 * @brief Get number of memory blocks used in a Memory Pool.
 */
uint32_t osMemoryPoolGetCount(osMemoryPoolId_t mp_id)
{
	struct cmsis_rtos_mempool_cb *mslab = (struct cmsis_rtos_mempool_cb *)mp_id;

	if (mslab == NULL) {
		return 0;
	} else {
		return k_mem_slab_num_used_get(&mslab->z_mslab);
	}
}

/**
 * @brief Get number of memory blocks available in a Memory Pool.
 */
uint32_t osMemoryPoolGetSpace(osMemoryPoolId_t mp_id)
{
	struct cmsis_rtos_mempool_cb *mslab = (struct cmsis_rtos_mempool_cb *)mp_id;

	if (mslab == NULL) {
		return 0;
	} else {
		return k_mem_slab_num_free_get(&mslab->z_mslab);
	}
}

/**
 * @brief Delete a Memory Pool object.
 */
osStatus_t osMemoryPoolDelete(osMemoryPoolId_t mp_id)
{
	struct cmsis_rtos_mempool_cb *mslab = (struct cmsis_rtos_mempool_cb *)mp_id;

	if (mslab == NULL) {
		return osErrorParameter;
	}

	if (k_is_in_isr()) {
		return osErrorISR;
	}

	/* The status code "osErrorResource" (the memory pool specified by
	 * parameter mp_id is in an invalid memory pool state) is not
	 * supported in Zephyr.
	 */

	if (mslab->is_dynamic_allocation) {
		k_free(mslab->pool);
	}
	if (mslab->is_cb_dynamic_allocation) {
		k_mem_slab_free(&cv2_mem_slab, (void *)mslab);
	}
	return osOK;
}
