#pragma once

#include "window.h"
#include "descriptor.h"

#include <imgui.h>
#include <imgui.cpp>
#include <imgui_draw.cpp>
#include <imgui_demo.cpp>
#include <imgui_widgets.cpp>
#include <imgui_impl_win32.h>
#include <imgui_impl_win32.cpp>
#include <imgui_impl_dx12.h>
#include <imgui_impl_dx12.cpp>

namespace detail
{
	void sp_debug_gui_init(ID3D12Device* device, const sp_window& window, sp_descriptor_handle font_descriptor_handle)
	{
		IMGUI_CHECKVERSION();

		ImGui::CreateContext();

		ImGui_ImplWin32_Init(window._handle);
		ImGui_ImplDX12_Init(device, 2, DXGI_FORMAT_R8G8B8A8_UNORM, font_descriptor_handle._handle_cpu_d3d12, font_descriptor_handle._handle_gpu_d3d12);
	}

	void sp_debug_gui_shutdown()
	{
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();

		ImGui::DestroyContext();
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
}

void sp_debug_gui_show_demo_window()
{
	bool show = true;
	ImGui::ShowDemoWindow(&show);
}