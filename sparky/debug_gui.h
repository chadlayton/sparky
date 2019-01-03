#pragma once

namespace detail
{
	void sp_debug_gui_init(ID3D12Device* device, const struct sp_window& window, struct sp_descriptor_handle font_descriptor_handle);
	void sp_debug_gui_shutdown();
	void sp_debug_gui_begin_frame();
	void sp_debug_gui_record_draw_commands(struct sp_graphics_command_list& comand_list);
}

void sp_debug_gui_show_demo_window();