#pragma once

#include <cstdlib>
#include <cstdint>
#include <cassert>
#include <intsafe.h>

struct sp_handle
{
	short index = SHORT_MAX;

	operator bool() const { return index != SHORT_MAX; };
};

struct sp_handle_pool
{
	short _count = 0;
	short _capacity = 0;
	sp_handle* _dense = nullptr;
	sp_handle* _sparse = nullptr;
};

void sp_handle_pool_create(sp_handle_pool* handle_pool, int capacity)
{
	assert(capacity < SHORT_MAX && "capacity too large");

	handle_pool->_dense = new sp_handle[capacity];
	handle_pool->_sparse = new sp_handle[capacity];
	handle_pool->_capacity = capacity;
	handle_pool->_count = 0;

	sp_handle* dense = handle_pool->_dense;
	for (int i = 0; i < capacity; i++)
	{
		dense[i].index = i;
	}
}

void sp_handle_pool_destroy(sp_handle_pool* pool)
{
	if (pool) 
	{
		delete[] pool->_dense;
		pool->_dense = nullptr;

		delete[] pool->_sparse;
		pool->_sparse = nullptr;

		pool->_count = 0;
		pool->_capacity = 0;
	}
}

sp_handle sp_handle_alloc(sp_handle_pool* pool)
{
	sp_handle handle;

	if (pool->_count < pool->_capacity) 
	{
		handle = pool->_dense[pool->_count];
		pool->_sparse[handle.index] = { pool->_count };
		++pool->_count;
	}
	else 
	{
		assert(0 && "handle pool is full");
	}

	return handle;
}

void sp_handle_free(sp_handle_pool* pool, sp_handle handle)
{
	assert(pool->_count > 0 && "handle pool is empty");

	--pool->_count;
	sp_handle dense_handle = pool->_dense[pool->_count];
	sp_handle sparse_handle = pool->_sparse[handle.index];
	pool->_dense[pool->_count] = handle;
	pool->_sparse[dense_handle.index] = sparse_handle;
	pool->_dense[sparse_handle.index] = dense_handle;
}

void sp_handle_pool_reset(sp_handle_pool* pool)
{
	sp_handle* dense = pool->_dense;
	for (int i = 0; i < pool->_capacity; i++)
	{
		dense[i].index = i;
	}
	pool->_count = 0;
}