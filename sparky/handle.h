#pragma once

#include <cstdlib>
#include <cstdint>
#include <cassert>

struct sp_handle
{
	short index = SHORT_MAX;

	operator bool() const { return index != SHORT_MAX; };
};

struct sp_handle_pool
{
	short count = 0;
	short capacity = 0;
	sp_handle* dense = nullptr;
	sp_handle* sparse = nullptr;
};

void sp_handle_pool_init(sp_handle_pool* handle_pool, int capacity)
{
	assert(capacity < SHORT_MAX && "capacity too large");

	handle_pool->dense = new sp_handle[capacity];
	handle_pool->sparse = new sp_handle[capacity];
	handle_pool->capacity = capacity;
	handle_pool->count = 0;

	sp_handle* dense = handle_pool->dense;
	for (int i = 0; i < capacity; i++)
	{
		dense[i].index = i;
	}
}

void sp_handle_pool_destroy(sp_handle_pool* pool)
{
	if (pool) 
	{
		delete[] pool->dense;
		pool->dense = nullptr;

		delete[] pool->sparse;
		pool->sparse = nullptr;

		pool->count = 0;
		pool->capacity = 0;
	}
}

sp_handle sp_handle_alloc(sp_handle_pool* pool)
{
	sp_handle handle;

	if (pool->count < pool->capacity) 
	{
		handle = pool->dense[pool->count];
		pool->sparse[handle.index] = { pool->count };
		++pool->count;
	}
	else 
	{
		assert(0 && "handle pool is full");
	}

	return handle;
}

void sp_handle_free(sp_handle_pool* pool, sp_handle handle)
{
	assert(pool->count > 0 && "handle pool is empty");

	--pool->count;
	sp_handle dense_handle = pool->dense[pool->count];
	sp_handle sparse_handle = pool->sparse[handle.index];
	pool->dense[pool->count] = handle;
	pool->sparse[dense_handle.index] = sparse_handle;
	pool->dense[sparse_handle.index] = dense_handle;
}

void sp_handle_pool_clear(sp_handle_pool* pool)
{
	sp_handle* dense = pool->dense;
	for (int i = 0; i < pool->capacity; i++)
	{
		dense[i].index = i;
	}
	pool->count = 0;
}