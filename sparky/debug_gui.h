#pragma once

namespace detail
{
	void sp_debug_gui_init(ID3D12Device* device, const struct sp_window& window, struct sp_descriptor_handle font_descriptor_handle);
	void sp_debug_gui_shutdown();
}