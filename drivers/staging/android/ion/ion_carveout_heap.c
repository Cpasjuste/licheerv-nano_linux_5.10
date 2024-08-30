// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2011 Google, Inc.
 */
#include <linux/spinlock.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/genalloc.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "ion.h"

#define ION_CARVEOUT_ALLOCATE_FAIL	-1

struct ion_carveout_heap {
	struct ion_heap heap;
	struct gen_pool *pool;
	phys_addr_t base;
};

static phys_addr_t ion_carveout_allocate(struct ion_heap *heap,
					 unsigned long size)
{
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);
	unsigned long offset = gen_pool_alloc(carveout_heap->pool, size);

	if (!offset)
		return ION_CARVEOUT_ALLOCATE_FAIL;

	return offset;
}

static void ion_carveout_free(struct ion_heap *heap, phys_addr_t addr,
			      unsigned long size)
{
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);

	if (addr == ION_CARVEOUT_ALLOCATE_FAIL)
		return;
	gen_pool_free(carveout_heap->pool, addr, size);
}

extern int cvi_ion_dump_heap_info(struct ion_heap *heap);
static int ion_carveout_heap_allocate(struct ion_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long size,
				      unsigned long flags)
{
	struct sg_table *table;
	phys_addr_t paddr;
	int ret;

	table = kmalloc(sizeof(*table), GFP_KERNEL);
	if (!table)
		return -ENOMEM;

	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret) {
		//pr_debug("%s sg_alloc_table failed!\n", __func__);
		goto err_free;
	}

	paddr = ion_carveout_allocate(heap, size);
	if (paddr == ION_CARVEOUT_ALLOCATE_FAIL) {
		ret = -ENOMEM;
		goto err_free_table;
	}

	sg_set_page(table->sgl, pfn_to_page(PFN_DOWN(paddr)), size, 0);
	buffer->sg_table = table;
#ifdef CONFIG_ION_CVITEK
	buffer->paddr = paddr;
#endif
	return 0;

err_free_table:
	sg_free_table(table);
err_free:
	kfree(table);
	return ret;
}

static void ion_carveout_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	struct sg_table *table = buffer->sg_table;
	struct page *page = sg_page(table->sgl);
	phys_addr_t paddr = PFN_PHYS(page_to_pfn(page));

	//remove it for saving cpu usage and vmap/unvmap frequency
	//ion_heap_buffer_zero(buffer);
#ifdef CONFIG_ION_CVITEK
	if (buffer->name)
		vfree(buffer->name);
#endif
	ion_carveout_free(heap, paddr, buffer->size);
	sg_free_table(table);
	kfree(table);
}

static struct ion_heap_ops carveout_heap_ops = {
	.allocate = ion_carveout_heap_allocate,
	.free = ion_carveout_heap_free,
	.map_user = ion_heap_map_user,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
};

struct ion_heap *ion_carveout_heap_create(struct ion_platform_heap *heap_data)
{
	struct ion_carveout_heap *carveout_heap;
	int ret;

	struct page *page;
	size_t size;

	page = pfn_to_page(PFN_DOWN(heap_data->base));
	size = heap_data->size;

	ret = ion_heap_pages_zero(page, size, pgprot_writecombine(PAGE_KERNEL));
	if (ret)
		return ERR_PTR(ret);

	carveout_heap = kzalloc(sizeof(*carveout_heap), GFP_KERNEL);
	if (!carveout_heap)
		return ERR_PTR(-ENOMEM);

	carveout_heap->pool = gen_pool_create(PAGE_SHIFT, -1);
	if (!carveout_heap->pool) {
		kfree(carveout_heap);
		return ERR_PTR(-ENOMEM);
	}

//	gen_pool_set_algo(carveout_heap->pool, gen_pool_best_fit, NULL);
	carveout_heap->base = heap_data->base;
	gen_pool_add(carveout_heap->pool, carveout_heap->base, heap_data->size,
		     -1);
	carveout_heap->heap.ops = &carveout_heap_ops;
	carveout_heap->heap.type = ION_HEAP_TYPE_CARVEOUT;
#ifndef CONFIG_ION_CVITEK
	carveout_heap->heap.flags = ION_HEAP_FLAG_DEFER_FREE;
#endif
	carveout_heap->heap.name = heap_data->name;
#ifdef CONFIG_ION_CVITEK
	carveout_heap->heap.total_size = heap_data->size;
#endif
	return &carveout_heap->heap;
}

int cvi_ion_dump_heap_info(struct ion_heap *heap)
{
	struct ion_device *dev = heap->dev;
	struct ion_buffer *pos, *n;
	size_t total_size = heap->total_size;
	size_t alloc_size;
	u64 alloc_bytes_wm;
	int usage_rate = 0;
	int rem;
	u64 tmp;

	spin_lock(&heap->stat_lock);
	alloc_size = heap->num_of_alloc_bytes;
	alloc_bytes_wm = heap->alloc_bytes_wm;
	spin_unlock(&heap->stat_lock);

	pr_err("Summary:\n");
	//In 4K(0x1 << 12) units
	tmp = (uint64_t)alloc_size * 100 >> 12;
	rem = do_div(tmp, total_size >> 12);
	usage_rate = tmp;
	if (rem)
		usage_rate += 1;

	pr_debug("[%d] %s heap size:%zu bytes, used:%zu bytes\n"
			, heap->id, heap->name, total_size, alloc_size);

	pr_debug("usage rate:%d%%, memory usage peak %llu bytes\n"
			, usage_rate, alloc_bytes_wm);

	pr_debug("\nDetails:\n%16s %16s %16s %16s %16s\n", "heap_id"
			, "alloc_buf_size", "phy_addr", "kmap_cnt", "buffer name");
	mutex_lock(&dev->buffer_lock);
	spin_lock(&heap->stat_lock);
	rbtree_postorder_for_each_entry_safe(pos, n, &dev->buffers, node) {
		/* only heap id matches will show buffer info */
		if (heap->id == pos->heap->id)
			pr_debug("%16d %16zu %16llx %16d %16s\n"
					, pos->heap->id, pos->size, pos->paddr,
					pos->kmap_cnt, pos->name);
	}
	spin_unlock(&heap->stat_lock);
	mutex_unlock(&dev->buffer_lock);

	return 0;
}
