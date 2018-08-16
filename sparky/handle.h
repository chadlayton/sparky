#pragma once

#include <cstdlib>
#include <cstdint>
#include <cassert>

typedef uint16_t sp_handle;

#define sp_INVALID_HANDLE UINT16_MAX;

struct sp_handle_pool
{
	int count = 0;
	int capacity = 0;
	sp_handle* dense = nullptr;
	sp_handle* sparse = nullptr;
};

void sp_handle_pool_init(sp_handle_pool* handle_pool, int capacity)
{
	assert(capacity < UINT16_MAX && "capacity too large");

	handle_pool->dense = new sp_handle[capacity];
	handle_pool->sparse = new sp_handle[capacity];
	handle_pool->capacity = capacity;
	handle_pool->count = 0;

	sp_handle* dense = handle_pool->dense;
	for (int i = 0; i < capacity; i++)
	{
		dense[i] = i;
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
	if (pool->count < pool->capacity) 
	{
		sp_handle index = pool->count;
		++pool->count;
		sp_handle handle = pool->dense[index];
		pool->sparse[handle] = index;
		return handle;
	}
	else 
	{
		assert(0 && "handle pool is full");
	}

	return sp_INVALID_HANDLE;
}

void sp_handle_free(sp_handle_pool* pool, sp_handle handle)
{
	assert(pool->count > 0);

	sp_handle index = pool->sparse[handle];
	--pool->count;
	sp_handle tmp = pool->dense[pool->count];
	pool->dense[pool->count] = handle;
	pool->sparse[tmp] = index;
	pool->dense[index] = tmp;
}

void sp_handle_pool_clear(sp_handle_pool* pool)
{
	sp_handle* dense = pool->dense;
	for (int i = 0; i < pool->capacity; i++)
	{
		dense[i] = i;
	}
	pool->count = 0;
}