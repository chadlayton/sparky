#pragma once

struct sp_window;
struct sp_descriptor_handle;
struct sp_graphics_command_list;

namespace detail
{
	void sp_debug_gui_init(ID3D12Device* device, const sp_window& window, sp_descriptor_handle font_descriptor_handle);
	void sp_debug_gui_shutdown();
	void sp_debug_gui_begin_frame();
	void sp_debug_gui_record_draw_commands(sp_graphics_command_list& comand_list);
}

void sp_debug_gui_show_demo_window();