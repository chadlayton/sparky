#pragma once

#include <array>

#include <d3d12.h>

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

sp_graphics_command_list sp_graphics_command_list_create(const char* name, const sp_graphics_command_list_desc& desc)
{
	sp_graphics_command_list command_list;

	HRESULT hr = _sp._device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&command_list._command_allocator_d3d12));
	assert(SUCCEEDED(hr));

#if GRAPHICS_OBJECT_DEBUG_NAMING_ENABLED
	command_list._command_allocator_d3d12->SetName(std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(name).c_str());
#endif

	ID3D12PipelineState* pipeline_state_d3d12 = nullptr;
	if (desc.pipeline_state_handle)
	{
		pipeline_state_d3d12 = sp_graphics_pipeline_state_get_impl(desc.pipeline_state_handle);
	}

	hr = _sp._device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		command_list._command_allocator_d3d12.Get(),
		pipeline_state_d3d12,
		IID_PPV_ARGS(&command_list._command_list_d3d12));
	assert(SUCCEEDED(hr));

#if GRAPHICS_OBJECT_DEBUG_NAMING_ENABLED
	command_list._command_list_d3d12->SetName(std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(name).c_str());
#endif

	command_list._name = name;

	return command_list;
}

void sp_graphics_command_list_destroy(sp_graphics_command_list& command_list)
{
	command_list._name = nullptr;
	command_list._command_list_d3d12.Reset();
	command_list._command_allocator_d3d12.Reset();
}

ID3D12GraphicsCommandList* sp_graphics_command_list_get_impl(const sp_graphics_command_list& command_list)
{
	return command_list._command_list_d3d12.Get();
}

void sp_graphics_command_list_reset(sp_graphics_command_list& command_list)
{
	HRESULT hr = command_list._command_allocator_d3d12->Reset();
	assert(SUCCEEDED(hr));

	hr = command_list._command_list_d3d12->Reset(
		command_list._command_allocator_d3d12.Get(),
		nullptr);
	assert(SUCCEEDED(hr));
}

void sp_graphics_command_list_reset(sp_graphics_command_list& command_list, const sp_graphics_pipeline_state_handle& pipeline_state_handle)
{
	HRESULT hr = command_list._command_allocator_d3d12->Reset();
	assert(SUCCEEDED(hr));

	hr = command_list._command_list_d3d12->Reset(
		command_list._command_allocator_d3d12.Get(),
		sp_graphics_pipeline_state_get_impl(pipeline_state_handle));
	assert(SUCCEEDED(hr));
}

namespace detail
{
	void sp_graphics_command_list_restore_default_resource_states(sp_graphics_command_list& command_list)
	{
		if (command_list._resource_transition_records_count > 0)
		{
			D3D12_RESOURCE_BARRIER render_target_transitions[64];

			memcpy(&render_target_transitions, command_list._resource_transition_records, command_list._resource_transition_records_count * sizeof(D3D12_RESOURCE_BARRIER));

			for (int i = 0; i < command_list._resource_transition_records_count; ++i)
			{
				std::swap(render_target_transitions[i].Transition.StateBefore, render_target_transitions[i].Transition.StateAfter);
			}

			command_list._command_list_d3d12->ResourceBarrier(command_list._resource_transition_records_count, render_target_transitions);

			command_list._resource_transition_records_count = 0;
		}
	}
}

void sp_graphics_command_list_set_vertex_buffers(sp_graphics_command_list& command_list, sp_vertex_buffer_handle* vertex_buffer_handles, unsigned vertex_buffer_count)
{
	D3D12_VERTEX_BUFFER_VIEW vertex_buffer_views[D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = { 0 };
	for (unsigned i = 0; i < vertex_buffer_count; ++i)
	{
		const sp_vertex_buffer& buffer = _sp.vertex_buffers[vertex_buffer_handles[i].index];

		memcpy(&vertex_buffer_views[i], &buffer._vertex_buffer_view, sizeof(D3D12_VERTEX_BUFFER_VIEW));
	}

	command_list._command_list_d3d12->IASetVertexBuffers(0, vertex_buffer_count, vertex_buffer_views);
}

void sp_graphics_command_list_set_render_targets(sp_graphics_command_list& command_list, sp_texture_handle* render_target_handles, int render_target_count, sp_texture_handle depth_stencil_handle)
{
	detail::sp_graphics_command_list_restore_default_resource_states(command_list);

	D3D12_CPU_DESCRIPTOR_HANDLE render_target_views[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT];

	for (int i = 0; i < render_target_count; ++i)
	{
		const sp_texture& texture = _sp.textures[render_target_handles[i].index];

		render_target_views[i] = texture._render_target_view;
	}

	// XXX: How do I track previous state? I can't put it on the resource itself because we might be touching it from multiple threads.
	// So put it on the command list? But those can be recorded in any order. Right now I just assume everything is in default state 
	// and transition from there. The command list records any transitions from the default state and we restore them before any new
	// ones. This means some wasted transitions but maybe we don't care. Next would probably be a frame graph?
	D3D12_RESOURCE_BARRIER render_target_transitions[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT + 1];

	for (int i = 0; i < render_target_count; ++i)
	{
		const sp_texture& texture = _sp.textures[render_target_handles[i].index];

		render_target_transitions[i] = CD3DX12_RESOURCE_BARRIER::Transition(texture._resource.Get(), texture._default_state, D3D12_RESOURCE_STATE_RENDER_TARGET);
	}

	int set_render_target_transitions_count = render_target_count;

	const D3D12_CPU_DESCRIPTOR_HANDLE* depth_stencil_view = nullptr;

	if (depth_stencil_handle)
	{
		const sp_texture& texture = _sp.textures[depth_stencil_handle.index];

		render_target_transitions[set_render_target_transitions_count] = CD3DX12_RESOURCE_BARRIER::Transition(texture._resource.Get(), texture._default_state, D3D12_RESOURCE_STATE_DEPTH_WRITE);

		++set_render_target_transitions_count;

		depth_stencil_view = &texture._depth_stencil_view;
	}

	memcpy(&command_list._resource_transition_records, render_target_transitions, set_render_target_transitions_count * sizeof(D3D12_RESOURCE_BARRIER));
	command_list._resource_transition_records_count = set_render_target_transitions_count;

	command_list._command_list_d3d12->ResourceBarrier(set_render_target_transitions_count, &render_target_transitions[0]);

	command_list._command_list_d3d12->OMSetRenderTargets(
		render_target_count,
		render_target_views,
		false,
		depth_stencil_view);
}

void sp_graphics_command_list_close(sp_graphics_command_list& command_list)
{
	detail::sp_graphics_command_list_restore_default_resource_states(command_list);

	HRESULT hr = command_list._command_list_d3d12->Close();
	assert(SUCCEEDED(hr));
}