#pragma once

/*
sp_frame_graph_script script;

// TODO: Could probably do away with this if willing to do named lookups
struct gbuffer_task_data
{
	sp_frame_graph_texture base_color;
	sp_frame_graph_texture normals;
};

sp_frame_graph_script_graphics_task_create<gbuffer_task_data>(
	&script, 
	"gbuffer",
	[&] (sp_frame_task_resource_builder& builder, gbuffer_task_data& data)
	{
		data.base_color = builder.create_render_target("base_color", ...);
		data.normals = builder.create_render_target("normals", ...);
	},
	[&] (sp_frame_task_resources& resources, sp_graphics_command_list& command_list, gbuffer_task_data& data)
	{
		
	});
*/