#pragma once

#include "handle.h"
#include "pipeline.h"
#include "command_list.h"

#include <d3d12.h>
#include <wrl.h>

struct sp_graphics_command_list_desc
{
	sp_graphics_pipeline_state_handle pipeline_state_handle;
};

struct sp_graphics_command_list
{
	const char* _name;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> _command_list_d3d12;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> _command_allocator_d3d12;

	D3D12_RESOURCE_BARRIER _resource_transition_records[64];
	int _resource_transition_records_count = 0;
};

sp_graphics_command_list sp_graphics_command_list_create(const char* name, const sp_graphics_command_list_desc& desc);
void sp_graphics_command_list_destroy(sp_graphics_command_list& command_list);