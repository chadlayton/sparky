#pragma once

#include "descriptor.h"
#include "sparky.h"
#include "command_list.h"

// TODO: Best practice for adding header only library to header only library? Just throw it in the directory?
#include "../../third_party/imgui/imgui.h"
#include "../../third_party/imgui/imgui_draw.cpp"
#include "../../third_party/imgui/imgui_impl_win32.h"
#include "../../third_party/imgui/imgui_impl_dx12.h"

#include "../../third_party/imgui/imgui.cpp"
#include "../../third_party/imgui/imgui_widgets.cpp"
#include "../../third_party/imgui/imgui_impl_win32.cpp"
#include "../../third_party/imgui/imgui_impl_dx12.cpp"

namespace detail
{
	void sp_debug_gui_init(void* window_handle, sp_descriptor_handle font_descriptor_handle)
	{
		IMGUI_CHECKVERSION();

		ImGui::CreateContext();

		ImGui_ImplWin32_Init(window_handle);
		ImGui_ImplDX12_Init(_sp._device.Get(), 2, DXGI_FORMAT_R10G10B10A2_UNORM, detail::_sp._descriptor_heap_cbv_srv_uav_gpu._heap_d3d12.Get(), font_descriptor_handle._handle_cpu_d3d12, font_descriptor_handle._handle_gpu_d3d12);
	}

	void sp_debug_gui_begin_frame()
	{
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
	}

	void sp_debug_gui_record_draw_commands(sp_graphics_command_list& comand_list)
	{
		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), comand_list._command_list_d3d12.Get());
	}

	void sp_debug_gui_shutdown()
	{
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();

		ImGui::DestroyContext();
	}
}