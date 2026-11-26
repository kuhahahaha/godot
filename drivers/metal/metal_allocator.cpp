/**************************************************************************/
/*  metal_allocator.cpp                                                   */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "drivers/metal/metal_allocator.h"

namespace {

class SpinLockGuard {
	const SpinLock &lock;

public:
	explicit SpinLockGuard(const SpinLock &p_lock) :
			lock(p_lock) {
		lock.lock();
	}
	~SpinLockGuard() {
		lock.unlock();
	}
};

} // namespace

#pragma mark - MetalAllocator

MetalAllocator *MetalAllocator::create(MTL::Device *p_device, bool p_use_heaps, bool p_local_device) {
	if (p_use_heaps) {
		return memnew(MetalHeapAllocator(p_device, p_local_device));
	}
	return memnew(MetalDeviceAllocator(p_device));
}

#pragma mark - MetalDeviceAllocator

MetalBuffer MetalDeviceAllocator::new_buffer(NS::UInteger p_length, MTL::ResourceOptions p_options) {
	MetalBuffer result;
	result.buffer = NS::TransferPtr(device->newBuffer(p_length, p_options));
	return result;
}

MetalTexture MetalDeviceAllocator::new_texture(const MTL::TextureDescriptor *p_desc) {
	MetalTexture result;
	result.texture = NS::TransferPtr(device->newTexture(p_desc));
	return result;
}

void MetalDeviceAllocator::free_buffer(MetalBuffer &p_buffer) {
	DEV_ASSERT(!p_buffer.allocation.is_valid());
	p_buffer.buffer.reset();
}

void MetalDeviceAllocator::free_texture(MetalTexture &p_texture) {
	DEV_ASSERT(!p_texture.allocation.is_valid());
	p_texture.texture.reset();
}

uint64_t MetalDeviceAllocator::get_heaps(LocalVector<MTL::Heap *> &r_heaps) {
	return 0;
}

uint64_t MetalDeviceAllocator::get_heap_generation() const {
	return 0;
}

#ifdef DEBUG_ENABLED
void MetalDeviceAllocator::get_stats(MetalAllocatorStats &r_stats) {
	r_stats = MetalAllocatorStats();
}
#endif

#pragma mark - MetalHeapAllocator

namespace {
constexpr NS::UInteger RESOURCE_CPU_CACHE_MODE_SHIFT = 0;
constexpr NS::UInteger RESOURCE_CPU_CACHE_MODE_MASK = 0xFull << RESOURCE_CPU_CACHE_MODE_SHIFT;
constexpr NS::UInteger RESOURCE_STORAGE_MODE_SHIFT = 4;
constexpr NS::UInteger RESOURCE_STORAGE_MODE_MASK = 0xFull << RESOURCE_STORAGE_MODE_SHIFT;
} // namespace

std::atomic<uint64_t> MetalHeapAllocator::total_buffer_recycle_hits{ 0 };

uint64_t MetalHeapAllocator::_preferred_block_size(uint32_t p_pool) const {
	if (local_device) {
		switch (p_pool) {
			case POOL_PRIVATE:
				return 8ull * 1024 * 1024;
			case POOL_SHARED:
			case POOL_SHARED_WC:
				return 4ull * 1024 * 1024;
			case POOL_PRIVATE_SMALL:
			case POOL_SHARED_SMALL:
			case POOL_SHARED_WC_SMALL:
				return 1ull * 1024 * 1024;
			default:
				return 4ull * 1024 * 1024;
		}
	}
	switch (p_pool) {
		case POOL_PRIVATE:
			return 16ull * 1024 * 1024;
		case POOL_SHARED:
		case POOL_SHARED_WC:
			return 8ull * 1024 * 1024;
		case POOL_PRIVATE_SMALL:
		case POOL_SHARED_SMALL:
		case POOL_SHARED_WC_SMALL:
			return 2ull * 1024 * 1024;
		default:
			return 8ull * 1024 * 1024;
	}
}

bool MetalHeapAllocator::_is_small_pool(uint32_t p_pool) {
	return p_pool == POOL_PRIVATE_SMALL || p_pool == POOL_SHARED_SMALL || p_pool == POOL_SHARED_WC_SMALL;
}

uint64_t MetalHeapAllocator::_max_recycled_bytes(uint32_t p_pool) const {
	if (local_device) {
		switch (p_pool) {
			case POOL_PRIVATE:
				return 8ull * 1024 * 1024;
			case POOL_SHARED:
			case POOL_SHARED_WC:
				return 4ull * 1024 * 1024;
			case POOL_PRIVATE_SMALL:
			case POOL_SHARED_SMALL:
			case POOL_SHARED_WC_SMALL:
				return 1ull * 1024 * 1024;
			default:
				return 4ull * 1024 * 1024;
		}
	}
	switch (p_pool) {
		case POOL_PRIVATE:
			return 16ull * 1024 * 1024;
		case POOL_SHARED:
		case POOL_SHARED_WC:
			return 8ull * 1024 * 1024;
		case POOL_PRIVATE_SMALL:
		case POOL_SHARED_SMALL:
		case POOL_SHARED_WC_SMALL:
			return 2ull * 1024 * 1024;
		default:
			return 8ull * 1024 * 1024;
	}
}

uint32_t MetalHeapAllocator::_pool_for_options(MTL::ResourceOptions p_options) {
	MTL::StorageMode storage = (MTL::StorageMode)((p_options & RESOURCE_STORAGE_MODE_MASK) >> RESOURCE_STORAGE_MODE_SHIFT);
	MTL::CPUCacheMode cache = (MTL::CPUCacheMode)((p_options & RESOURCE_CPU_CACHE_MODE_MASK) >> RESOURCE_CPU_CACHE_MODE_SHIFT);
	if (storage == MTL::StorageModeShared) {
		return cache == MTL::CPUCacheModeWriteCombined ? POOL_SHARED_WC : POOL_SHARED;
	}
	// Private and Memoryless both land in the private pool.
	return POOL_PRIVATE;
}

uint32_t MetalHeapAllocator::select_pool(MTL::ResourceOptions p_options, uint64_t p_length) {
	const uint32_t base = _pool_for_options(p_options);
	if (p_length <= SMALL_ALLOC_THRESHOLD) {
		switch (base) {
			case POOL_PRIVATE:
				return POOL_PRIVATE_SMALL;
			case POOL_SHARED:
				return POOL_SHARED_SMALL;
			case POOL_SHARED_WC:
			default:
				return POOL_SHARED_WC_SMALL;
		}
	}
	return base;
}

uint32_t MetalHeapAllocator::_pool_for_texture(const MTL::TextureDescriptor *p_desc, uint64_t p_size) {
	// Memoryless textures may be placed into any heap:
	// newTextureWithDescriptor:offset: permits StorageModeMemoryless
	// regardless of the heap's storage mode.
	const bool is_shared = p_desc->storageMode() == MTL::StorageModeShared;
	if (p_size <= SMALL_ALLOC_THRESHOLD) {
		return is_shared ? POOL_SHARED_SMALL : POOL_PRIVATE_SMALL;
	}
	return is_shared ? POOL_SHARED : POOL_PRIVATE;
}

NS::SharedPtr<MTL::Heap> MetalHeapAllocator::_create_heap(uint32_t p_pool, uint64_t p_size) {
	NS::SharedPtr<MTL::HeapDescriptor> desc = NS::TransferPtr(MTL::HeapDescriptor::alloc()->init());
	desc->setType(MTL::HeapTypePlacement);
	const bool is_private = (p_pool == POOL_PRIVATE || p_pool == POOL_PRIVATE_SMALL);
	const bool is_wc = (p_pool == POOL_SHARED_WC || p_pool == POOL_SHARED_WC_SMALL);
	desc->setStorageMode(is_private ? MTL::StorageModePrivate : MTL::StorageModeShared);
	desc->setCpuCacheMode(is_wc ? MTL::CPUCacheModeWriteCombined : MTL::CPUCacheModeDefaultCache);
	desc->setHazardTrackingMode(MTL::HazardTrackingModeUntracked);
	desc->setSize(p_size);
	return NS::TransferPtr(device->newHeap(desc.get()));
}

bool MetalHeapAllocator::_pool_allocation_units(uint64_t p_size, uint64_t p_align, uint32_t &r_units) {
	r_units = 0;
	if (!p_size || !p_align || (p_align & (p_align - 1))) {
		return false;
	}
	const uint64_t padding = p_align > UNIT ? p_align - 1 : 0;
	if (p_size > UINT64_MAX - padding || p_size + padding > UINT64_MAX - (UNIT - 1)) {
		return false;
	}
	const uint64_t units = (p_size + padding + UNIT - 1) / UNIT;
	if (units > UINT32_MAX) {
		return false;
	}
	r_units = uint32_t(units);
	return true;
}

bool MetalHeapAllocator::_pool_allocate(uint32_t p_pool, uint64_t p_size, uint64_t p_align, MetalAllocation &r_allocation, MTL::Heap *&r_heap, uint64_t &r_offset) {
	Pool &pool = pools[p_pool];
	uint32_t units = 0;
	if (!_pool_allocation_units(p_size, p_align, units)) {
		return false;
	}

	SpinLockGuard lock(pool.mutex);

	uint32_t block_index = UINT32_MAX;
	OffsetAllocator::Allocation alloc{};
	for (uint32_t i = 0; i < pool.blocks.size(); i++) {
		if (pool.blocks[i].metadata == nullptr) {
			continue; // Reclaimed slot.
		}
		alloc = pool.blocks[i].metadata->allocate(units);
		if (alloc.offset != OffsetAllocator::Allocation::NO_SPACE) {
			block_index = i;
			break;
		}
	}

	bool is_new_block = false;
	if (block_index == UINT32_MAX) {
		is_new_block = true;
		uint64_t block_size = MAX(_preferred_block_size(p_pool), uint64_t(units) * UNIT);
		NS::SharedPtr<MTL::Heap> heap = _create_heap(p_pool, block_size);
		ERR_FAIL_NULL_V_MSG(heap.get(), false, "Unable to create placement heap.");

		const uint32_t block_units = (uint32_t)(block_size / UNIT);
		const uint32_t max_allocs = MAX(1u, MIN(MAX_ALLOCS_PER_BLOCK, block_units));
		Block block;
		block.heap = heap;
		block.metadata = memnew(OffsetAllocator::Allocator(block_units, max_allocs));

		// Reuse a reclaimed slot if one exists, else append. Block indices
		// stored in live handles must stay stable, so blocks are never erased.
		block_index = pool.blocks.size();
		for (uint32_t i = 0; i < pool.blocks.size(); i++) {
			if (pool.blocks[i].metadata == nullptr && pool.blocks[i].heap.get() == nullptr) {
				block_index = i;
				break;
			}
		}
		if (block_index == pool.blocks.size()) {
			pool.blocks.push_back(block);
		} else {
			pool.blocks[block_index] = block;
		}
#ifdef DEBUG_ENABLED
		pool.stats.block_count++;
		pool.stats.reserved_bytes += heap->size();
#endif

		alloc = pool.blocks[block_index].metadata->allocate(units);
		ERR_FAIL_COND_V(alloc.offset == OffsetAllocator::Allocation::NO_SPACE, false);
	}

	Block &block = pool.blocks[block_index];
	const bool was_active = (block.live > block.recycled_count);
	if (block.live == 0) {
		if (!is_new_block && pool.empty_blocks > 0) {
			pool.empty_blocks--;
		}
		block.empty_epoch = 0;
		if (block.purgeable_empty) {
			block.heap->setPurgeableState(MTL::PurgeableStateNonVolatile);
			block.purgeable_empty = false;
		}
	}
	block.live++;
	if (!was_active) {
		generation.fetch_add(1, std::memory_order_release);
	}
#ifdef DEBUG_ENABLED
	pool.stats.allocation_count++;
	pool.stats.used_bytes += (uint64_t)units * UNIT;
#endif

	uint64_t raw_offset = (uint64_t)alloc.offset * UNIT;
	r_offset = (raw_offset + p_align - 1) & ~(p_align - 1);
	r_heap = block.heap.get();
	r_allocation.kind = MetalAllocation::Kind::POOL;
	r_allocation.pool = (uint8_t)p_pool;
	r_allocation.block = block_index;
	r_allocation.alloc = alloc;
	return true;
}

bool MetalHeapAllocator::_dedicated_allocate(uint32_t p_pool, uint64_t p_size, MetalAllocation &r_allocation, MTL::Heap *&r_heap) {
	NS::SharedPtr<MTL::Heap> heap = _create_heap(p_pool, p_size);
	ERR_FAIL_NULL_V_MSG(heap.get(), false, "Unable to create dedicated placement heap.");

	Pool &pool = pools[p_pool];
	SpinLockGuard lock(pool.mutex);
	pool.dedicated_heaps.push_back(heap.get());
	heap->retain(); // dedicated_heaps holds an owned reference.
#ifdef DEBUG_ENABLED
	pool.stats.dedicated_count++;
	pool.stats.dedicated_bytes += heap->size();
#endif
	generation.fetch_add(1, std::memory_order_release);

	r_heap = heap.get();
	r_allocation.kind = MetalAllocation::Kind::DEDICATED;
	r_allocation.pool = (uint8_t)p_pool;
	r_allocation.heap = heap.get();
	r_allocation.heap->retain(); // Handle's owned reference, released in _free_allocation.
	return true;
}

void MetalHeapAllocator::_free_allocation(MetalAllocation &p_allocation) {
	// old_heap and evicted_block_buffers are released outside any locks.
	NS::SharedPtr<MTL::Heap> old_heap;
	LocalVector<MetalBuffer> evicted_block_buffers;

	switch (p_allocation.kind) {
		case MetalAllocation::Kind::INVALID:
			break;
		case MetalAllocation::Kind::POOL: {
			Pool &pool = pools[p_allocation.pool];
			SpinLockGuard lock(pool.mutex);
			Block &block = pool.blocks[p_allocation.block];
			const bool was_active = (block.live > block.recycled_count);
#ifdef DEBUG_ENABLED
			uint64_t freed_bytes = (uint64_t)block.metadata->allocationSize(p_allocation.alloc) * UNIT;
			pool.stats.allocation_count--;
			pool.stats.used_bytes -= freed_bytes;
#endif
			block.metadata->free(p_allocation.alloc);
			block.live--;
			if (block.live == 0) {
				pool.empty_blocks++;
				// Keep at most one empty block cached per pool; reclaim beyond that
				// immediately. The remaining empty block is omitted from get_heaps()
				// right away and reclaimed (regular pools) or purged (small pools)
				// by trim_idle().
				if (pool.empty_blocks > 1) {
#ifdef DEBUG_ENABLED
					pool.stats.block_count--;
					pool.stats.reserved_bytes -= block.heap->size();
#endif
					memdelete(block.metadata);
					block.metadata = nullptr;
					old_heap = std::move(block.heap);
					block.empty_epoch = 0;
					block.purgeable_empty = false;
					pool.empty_blocks--;
					generation.fetch_add(1, std::memory_order_release);
				} else {
					block.empty_epoch = idle_epoch.load(std::memory_order_relaxed);
					if (was_active) {
						generation.fetch_add(1, std::memory_order_release);
					}
				}
			} else if (block.live == block.recycled_count) {
				if (was_active) {
					generation.fetch_add(1, std::memory_order_release);
				}
				uint32_t live_blocks = 0;
				for (const Block &b : pool.blocks) {
					if (b.heap.get() != nullptr) {
						live_blocks++;
					}
				}
				if (live_blocks > 1) {
					const uint32_t target_block = p_allocation.block;
					uint32_t write_idx = 0;
					for (uint32_t i = 0; i < pool.recycled_buffers.size(); i++) {
						if (pool.recycled_buffers[i].buffer.allocation.kind == MetalAllocation::Kind::POOL &&
								pool.recycled_buffers[i].buffer.allocation.block == target_block) {
							pool.recycled_bytes -= pool.recycled_buffers[i].length;
							evicted_block_buffers.push_back(std::move(pool.recycled_buffers[i].buffer));
						} else {
							if (write_idx != i) {
								pool.recycled_buffers[write_idx] = std::move(pool.recycled_buffers[i]);
							}
							write_idx++;
						}
					}
					pool.recycled_buffers.resize(write_idx);
					block.recycled_count = 0;
				}
			}
		} break;
		case MetalAllocation::Kind::DEDICATED: {
			Pool &pool = pools[p_allocation.pool];
			SpinLockGuard lock(pool.mutex);
			int64_t idx = pool.dedicated_heaps.find(p_allocation.heap);
			DEV_ASSERT(idx >= 0);
			if (likely(idx >= 0)) {
#ifdef DEBUG_ENABLED
				pool.stats.dedicated_count--;
				pool.stats.dedicated_bytes -= p_allocation.heap->size();
#endif
				pool.dedicated_heaps[idx]->release(); // not released here, as p_allocation also has a reference
				pool.dedicated_heaps.remove_at_unordered(idx);
			}
			old_heap = NS::TransferPtr(p_allocation.heap);
			p_allocation.heap = nullptr;
			generation.fetch_add(1, std::memory_order_release);
		} break;
	}
	p_allocation.invalidate();

	for (MetalBuffer &old : evicted_block_buffers) {
		old.buffer.reset();
		_free_allocation(old.allocation);
	}
}

MetalBuffer MetalHeapAllocator::new_buffer(NS::UInteger p_length, MTL::ResourceOptions p_options) {
	// On local devices the regular shared/private pools are already compact
	// (4-8 MiB) and pre-warmed by RD init; avoid splitting <= 64 KiB scratch
	// buffers into extra 1 MiB sub-pool heaps.
	uint32_t pool_index = local_device ? _pool_for_options(p_options) : select_pool(p_options, p_length);
	// Placement-heap resources inherit the heap's hazard tracking; strip
	// hazard bits and pass storage/cache + untracked only.
	MTL::ResourceOptions heap_options = (p_options & (RESOURCE_STORAGE_MODE_MASK | RESOURCE_CPU_CACHE_MODE_MASK)) | MTL::ResourceHazardTrackingModeUntracked;

	if (p_length > 0) {
		Pool &pool = pools[pool_index];
		SpinLockGuard lock(pool.mutex);
		for (uint32_t i = pool.recycled_buffers.size(); i > 0; --i) {
			RecycledBuffer &entry = pool.recycled_buffers[i - 1];
			if (entry.length == p_length && entry.options == heap_options) {
				MetalBuffer recycled = std::move(entry.buffer);
				if (recycled.allocation.kind == MetalAllocation::Kind::POOL && recycled.allocation.block < pool.blocks.size()) {
					Block &block = pool.blocks[recycled.allocation.block];
					DEV_ASSERT(block.recycled_count > 0);
					const bool was_active = (block.live > block.recycled_count);
					block.recycled_count--;
					if (!was_active) {
						generation.fetch_add(1, std::memory_order_release);
					}
				}
				pool.recycled_bytes -= entry.length;
				const uint32_t last = pool.recycled_buffers.size() - 1;
				if (i - 1 != last) {
					pool.recycled_buffers[i - 1] = std::move(pool.recycled_buffers[last]);
				}
				pool.recycled_buffers.resize(last);
				buffer_recycle_hits.fetch_add(1, std::memory_order_relaxed);
				total_buffer_recycle_hits.fetch_add(1, std::memory_order_relaxed);
				return recycled;
			}
		}
	}

	MTL::SizeAndAlign sa = device->heapBufferSizeAndAlign(p_length, p_options);

	MetalBuffer result;
	MTL::Heap *heap = nullptr;
	if (sa.size > _preferred_block_size(pool_index) / 2) {
		if (!_dedicated_allocate(pool_index, sa.size, result.allocation, heap)) {
			return result;
		}
		result.buffer = NS::TransferPtr(heap->newBuffer(p_length, heap_options, 0));
	} else {
		uint64_t offset = 0;
		if (!_pool_allocate(pool_index, sa.size, sa.align, result.allocation, heap, offset)) {
			return result;
		}
		result.buffer = NS::TransferPtr(heap->newBuffer(p_length, heap_options, offset));
	}
	if (!result.buffer) {
		_free_allocation(result.allocation);
	}
	return result;
}

bool MetalHeapAllocator::texture_allocation_quote(const MTL::TextureDescriptor *p_desc, TextureAllocationQuote &r_quote) {
	r_quote = {};
	const MTL::SizeAndAlign sa = device->heapTextureSizeAndAlign(p_desc);
	if (!sa.size || !sa.align || (sa.align & (sa.align - 1))) {
		return false;
	}
	const uint32_t pool_index = local_device
			? (p_desc->storageMode() == MTL::StorageModeShared ? POOL_SHARED : POOL_PRIVATE)
			: _pool_for_texture(p_desc, sa.size);
	TextureAllocationQuote quote;
	quote.native_requirement = sa.size;
	quote.alignment = sa.align;
	quote.dedicated = sa.size > _preferred_block_size(pool_index) / 2;
	if (quote.dedicated) {
		quote.owner_reservation = sa.size;
	} else {
		uint32_t units = 0;
		if (!_pool_allocation_units(sa.size, sa.align, units)) {
			return false;
		}
		quote.owner_reservation = uint64_t(units) * UNIT;
	}
	r_quote = quote;
	return true;
}

MetalTexture MetalHeapAllocator::new_texture(const MTL::TextureDescriptor *p_desc) {
	TextureAllocationQuote quote;
	MetalTexture result;
	if (!texture_allocation_quote(p_desc, quote)) {
		return result;
	}
	uint32_t pool_index = local_device
			? (p_desc->storageMode() == MTL::StorageModeShared ? POOL_SHARED : POOL_PRIVATE)
			: _pool_for_texture(p_desc, quote.native_requirement);
	MTL::Heap *heap = nullptr;
	if (quote.dedicated) {
		if (!_dedicated_allocate(pool_index, quote.native_requirement, result.allocation, heap)) {
			return result;
		}
		result.texture = NS::TransferPtr(heap->newTexture(p_desc, 0));
	} else {
		uint64_t offset = 0;
		if (!_pool_allocate(pool_index, quote.native_requirement, quote.alignment, result.allocation, heap, offset)) {
			return result;
		}
		result.texture = NS::TransferPtr(heap->newTexture(p_desc, offset));
	}
	if (!result.texture) {
		_free_allocation(result.allocation);
	}
	return result;
}

void MetalHeapAllocator::free_buffer(MetalBuffer &p_buffer) {
	// Only recycle POOL allocations; DEDICATED single-resource heaps must be
	// released immediately so large buffers never pin dedicated heaps (BUG-83).
	if (p_buffer.buffer.get() != nullptr && p_buffer.allocation.kind == MetalAllocation::Kind::POOL && p_buffer.allocation.pool < POOL_COUNT) {
		const uint64_t length = p_buffer.buffer->length();
		const uint32_t pool_index = p_buffer.allocation.pool;
		const uint32_t block_index = p_buffer.allocation.block;
		const uint64_t max_bytes = _max_recycled_bytes(pool_index);
		if (length > 0 && length <= max_bytes) {
			const MTL::ResourceOptions heap_options = (p_buffer.buffer->resourceOptions() & (RESOURCE_STORAGE_MODE_MASK | RESOURCE_CPU_CACHE_MODE_MASK)) | MTL::ResourceHazardTrackingModeUntracked;
			const uint64_t epoch = idle_epoch.load(std::memory_order_relaxed);
			LocalVector<MetalBuffer> evicted;
			{
				Pool &pool = pools[pool_index];
				SpinLockGuard lock(pool.mutex);
				while (!pool.recycled_buffers.is_empty() &&
						(pool.recycled_buffers.size() >= MAX_RECYCLED_BUFFERS_PER_POOL || pool.recycled_bytes + length > max_bytes)) {
					RecycledBuffer &front = pool.recycled_buffers[0];
					if (front.buffer.allocation.kind == MetalAllocation::Kind::POOL && front.buffer.allocation.block < pool.blocks.size()) {
						DEV_ASSERT(pool.blocks[front.buffer.allocation.block].recycled_count > 0);
						pool.blocks[front.buffer.allocation.block].recycled_count--;
					}
					pool.recycled_bytes -= front.length;
					evicted.push_back(std::move(front.buffer));
					pool.recycled_buffers.remove_at(0);
				}
				RecycledBuffer entry;
				entry.length = length;
				entry.options = heap_options;
				entry.recycle_epoch = epoch;
				entry.buffer = std::move(p_buffer);
				pool.recycled_bytes += length;
				pool.recycled_buffers.push_back(std::move(entry));
				if (block_index < pool.blocks.size()) {
					Block &block = pool.blocks[block_index];
					const bool was_active = (block.live > block.recycled_count);
					block.recycled_count++;
					if (block.live > 0 && block.live == block.recycled_count) {
						if (was_active) {
							generation.fetch_add(1, std::memory_order_release);
						}
						uint32_t live_blocks = 0;
						for (const Block &b : pool.blocks) {
							if (b.heap.get() != nullptr) {
								live_blocks++;
							}
						}
						if (live_blocks > 1) {
							uint32_t write_idx = 0;
							for (uint32_t i = 0; i < pool.recycled_buffers.size(); i++) {
								if (pool.recycled_buffers[i].buffer.allocation.kind == MetalAllocation::Kind::POOL &&
										pool.recycled_buffers[i].buffer.allocation.block == block_index) {
									pool.recycled_bytes -= pool.recycled_buffers[i].length;
									evicted.push_back(std::move(pool.recycled_buffers[i].buffer));
								} else {
									if (write_idx != i) {
										pool.recycled_buffers[write_idx] = std::move(pool.recycled_buffers[i]);
									}
									write_idx++;
								}
							}
							pool.recycled_buffers.resize(write_idx);
							block.recycled_count = 0;
						}
					}
				}
			}
			for (MetalBuffer &old : evicted) {
				old.buffer.reset();
				_free_allocation(old.allocation);
			}
			p_buffer.buffer.reset();
			p_buffer.allocation.invalidate();
			return;
		}
	}
	p_buffer.buffer.reset();
	_free_allocation(p_buffer.allocation);
}

void MetalHeapAllocator::free_texture(MetalTexture &p_texture) {
	p_texture.texture.reset();
	_free_allocation(p_texture.allocation);
}

uint64_t MetalHeapAllocator::get_heaps(LocalVector<MTL::Heap *> &r_heaps) {
	uint64_t gen = generation.load(std::memory_order_acquire);
	for (uint32_t p = 0; p < POOL_COUNT; p++) {
		Pool &pool = pools[p];
		SpinLockGuard lock(pool.mutex);
		for (const Block &block : pool.blocks) {
			if (block.heap.get() != nullptr && block.live > block.recycled_count) {
				r_heaps.push_back(block.heap.get());
			}
		}
		for (MTL::Heap *heap : pool.dedicated_heaps) {
			r_heaps.push_back(heap);
		}
	}
	return gen;
}

uint64_t MetalHeapAllocator::get_heap_generation() const {
	return generation.load(std::memory_order_acquire);
}

void MetalHeapAllocator::trim_idle() {
	const uint64_t epoch = idle_epoch.fetch_add(1, std::memory_order_relaxed) + 1;
	LocalVector<MetalBuffer> evicted_buffers;
	LocalVector<NS::SharedPtr<MTL::Heap>> evicted_heaps;

	for (uint32_t p = 0; p < POOL_COUNT; p++) {
		Pool &pool = pools[p];
		SpinLockGuard lock(pool.mutex);
		if (pool.recycled_buffers.is_empty() && pool.empty_blocks == 0) {
			continue;
		}

		if (!pool.recycled_buffers.is_empty()) {
			uint32_t write_idx = 0;
			for (uint32_t i = 0; i < pool.recycled_buffers.size(); i++) {
				RecycledBuffer &entry = pool.recycled_buffers[i];
				if (epoch >= entry.recycle_epoch && (epoch - entry.recycle_epoch) >= RECYCLE_IDLE_EPOCHS) {
					if (entry.buffer.allocation.kind == MetalAllocation::Kind::POOL && entry.buffer.allocation.block < pool.blocks.size()) {
						DEV_ASSERT(pool.blocks[entry.buffer.allocation.block].recycled_count > 0);
						pool.blocks[entry.buffer.allocation.block].recycled_count--;
					}
					pool.recycled_bytes -= entry.length;
					evicted_buffers.push_back(std::move(entry.buffer));
				} else {
					if (write_idx != i) {
						pool.recycled_buffers[write_idx] = std::move(entry);
					}
					write_idx++;
				}
			}
			pool.recycled_buffers.resize(write_idx);
		}

		if (pool.empty_blocks > 0) {
			const bool is_small = _is_small_pool(p);
			for (Block &block : pool.blocks) {
				if (block.heap.get() == nullptr || block.live != 0) {
					continue;
				}
				if (epoch < block.empty_epoch || (epoch - block.empty_epoch) < EMPTY_BLOCK_IDLE_EPOCHS) {
					continue;
				}
				if (!is_small) {
#ifdef DEBUG_ENABLED
					pool.stats.block_count--;
					pool.stats.reserved_bytes -= block.heap->size();
#endif
					memdelete(block.metadata);
					block.metadata = nullptr;
					evicted_heaps.push_back(std::move(block.heap));
					block.empty_epoch = 0;
					block.purgeable_empty = false;
					pool.empty_blocks--;
					generation.fetch_add(1, std::memory_order_release);
				} else if (!block.purgeable_empty) {
					block.heap->setPurgeableState(MTL::PurgeableStateEmpty);
					block.purgeable_empty = true;
				}
			}
		}
	}

	evicted_heaps.clear();
	for (MetalBuffer &old : evicted_buffers) {
		old.buffer.reset();
		_free_allocation(old.allocation);
	}
}

#ifdef DEBUG_ENABLED
void MetalHeapAllocator::get_stats(MetalAllocatorStats &r_stats) {
	for (uint32_t p = 0; p < POOL_COUNT; p++) {
		SpinLockGuard lock(pools[p].mutex);
		r_stats.pools[p] = pools[p].stats;
	}
}
#endif

MetalHeapAllocator::~MetalHeapAllocator() {
	for (uint32_t p = 0; p < POOL_COUNT; p++) {
		Pool &pool = pools[p];
		for (Block &block : pool.blocks) {
			block.recycled_count = 0;
		}
		LocalVector<RecycledBuffer> drained = std::move(pool.recycled_buffers);
		pool.recycled_bytes = 0;
		for (RecycledBuffer &entry : drained) {
			entry.buffer.buffer.reset();
			_free_allocation(entry.buffer.allocation);
		}
		for (Block &block : pool.blocks) {
			if (block.metadata != nullptr) {
				memdelete(block.metadata);
			}
		}
		for (MTL::Heap *heap : pool.dedicated_heaps) {
			heap->release();
		}
	}
}
