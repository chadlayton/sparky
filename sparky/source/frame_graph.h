#pragma once

#include <optional>

struct sp_graphics_task
{
	const char* name = nullptr;

	sp_texture_handle render_targets[4];
	int render_target_count = 0;

	std::optional<sp_texture_handle> depth;
};

void sp_frame_graph_task_begin(sp_graphics_command_list& command_list, const sp_graphics_task& task)
{
	// each task has
	// [required] render targets / "attachments" + depth
	// [optional] resources (constants, buffers, textures)

	// bind render targets
	// clear them (maybe)
	// set them
	// set resources

	sp_graphics_command_list_set_render_targets(command_list, &task.render_targets[0], task.render_target_count, task.depth.value_or(sp_texture_handle{}));
	for (int i = 0; i < task.render_target_count; ++i)
	{
		sp_texture_handle render_target = task.render_targets[i];
		sp_graphics_command_list_clear_render_target(command_list, render_target);
	}
	if (task.depth)
	{
		sp_graphics_command_list_clear_depth(command_list, task.depth.value());
	}
}

//struct sp_frame_graph_desc
//{
//	sp_render_task_desc render_tasks[64];
//	int task_count = 0;
//};

//void sp_frame_graph_builder_add_render_task(sp_frame_graph_desc* desc, sp_render_task_desc& render_task_desc)
//{
//
//}

void sp_frame_graph_test()
{

}

// TASK GRAPH MUSINGS
//
// gbuffer_render_task = {
//   "gbuffer_render_task",
//   inputs = {
//   },
//   outputs = {
//     gbuffer1
//     gbuffer2
//     depth
//   },
//   submit = gbuffer_render_task_submit
// }
//
// material = {
//   {
//     "gbuffer_render_task",
//     { 
//       gbuffer.hlsl,
//       vertex_format = {
//         { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT },
//         { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT },
//         { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT },
//       }
//     }
//   },
//   {
//     "shadow_render_task"
//     {
//       shadow.hlsl,
//       vertex_format = {
//         { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT },
//       }
//     }
//   }
// }
//
// XXX: We could infer the vertex format with shader reflection.
//
// XXX: This design implies pipeline state becomes not-user facing. They get created automatically when we create a material
// which looks at the tasks for the render and depth target formats / count and the material for everything else.
//
// terrain_compute_task_submit()
// {
//   dispatch(terrain_compute_task);
// }
//
// A compute_task would be simliar to material in that we tell it up front what resources are going to be required. Creates
// a compute pipeline behind the scenes.
//
// XXX: May not even need to supply a submit func to compute tasks. Just provide all data and assume we're going
// to call dispatch.
// 
// gbuffer_render_task_submit()
// {
//   for (models : scene)
//   {
//     draw(model.vertex_stream, model.material);
//   }
//   
//   draw(terrain_vertex_buffers, terrain_material);
// }
//
// XXX: The draw call could validate the vertex stream/buffer format satisfies that expected by the material
//
// shadow_render_task_submit()
// {
//   for (models : scene)
//   {
//     draw(model.position_only_vertex_stream, model.material);
//   }
// }
//

/*
sp_frame_graph_desc desc;

sp_frame_graph_builder_add_texture_resource(&desc, "base_color", r8g8b8);
sp_frame_graph_builder_add_texture_resource(&desc, "depth", r8g8b8);

sp_grame_graph_task_desc gbuffer_task_desc = sp_frame_graph_builder_add_task(&desc, "gbuffer_task");

sp_frame_graph_task_builder_set_render_targets(&gbuffer_task_desc, { { "base_color", r8g8b8 } },

sp_frame_graph graph = sp_frame_graph_create(desc);

sp_frame_graph_task_desc* gbuffer_task = sp_frame_graph_task_create(&graph, "gbuffer_task");
gbuffer_task->add_output("base_color");
gbuffer_task->add_output("depth");


*/
