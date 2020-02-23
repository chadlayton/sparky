#include "../../../sparky/shaders/sparky/noise.hlsli"

#include "per_frame_cbuffer.hlsli"
#include "fullscreen_triangle.hlsli"
#include "position_from_depth.hlsli"
#include "tonemap.hlsli"
#include "gamma_correction.hlsli"
#include "clouds.cbuffer.hlsli"
#include "intersect.hlsli"

Texture2D gbuffer_depth_texture : register(t0);
Texture3D cloud_shape_texture : register(t1);
Texture3D cloud_detail_texture : register(t2);
Texture2D cloud_weather_texture : register(t3);

SamplerState default_sampler : register(s0);

#define PI      3.1415926f
#define EPSILON 0.000001f

//#define ENABLE_RAYMARCH_DEBUGGING

float remap(float original_value, float original_min, float original_max, float new_min, float new_max)
{
	return new_min + (((original_value - original_min) / (original_max - original_min)) * (new_max - new_min));
}

// cloud type controls the the height of the cloud which in turn defines how billowy vs how wispy it is
float apply_cloud_type(float density, float height_signal, float cloud_type)
{
	const float cumulus = saturate(remap(height_signal, 0.0, 0.2, 0.0, 1.0) * remap(height_signal, 0.7, 0.9, 1.0, 0.0));
	const float stratocumulus = saturate(remap(height_signal, 0.0, 0.2, 0.0, 1.0) * remap(height_signal, 0.2, 0.7, 1.0, 0.0));
	const float stratus = saturate(remap(height_signal, 0.0, 0.1, 0.0, 1.0) * remap(height_signal, 0.2, 0.3, 1.0, 0.0));

	const float stratus_to_stratocumulus = lerp(stratus, stratocumulus, saturate(cloud_type * 2.0));
	const float stratocumulus_to_cumulus = lerp(stratocumulus, cumulus, saturate((cloud_type - 0.5) * 2.0));

	return density * lerp(stratus_to_stratocumulus, stratocumulus_to_cumulus, cloud_type);
}

// cloud coverage decides the presence of a cloud
float apply_cloud_coverage(float density, float cloud_coverage)
{
	density = remap(density, min(1 - cloud_coverage, 1.0 - EPSILON), 1.0, 0.0, 1.0);

	density *= cloud_coverage;

	return density;
}

// The height signal ranges from 0 at the bottom of the cloud layer to 1 at the top
float cloud_layer_height_signal(float3 sample_position_ws)
{
	const float sample_height = sample_position_ws.y;

	return saturate((sample_height - cloud_layer_height_begin) / (cloud_layer_height_end - cloud_layer_height_begin));
}

float sample_cloud_density(float3 sample_position_ws)
{
	sample_position_ws.xz += 100000.0f;

	const float height_signal = cloud_layer_height_signal(sample_position_ws);

	const float cloud_shape_sample_scale_base = 0.0002f;
	const float cloud_shape_sample_scale = (cloud_shape_sample_scale_base * (1 - shape_sample_scale_bias));
	const float4 cloud_shape = cloud_shape_texture.Sample(default_sampler, sample_position_ws * cloud_shape_sample_scale);

	float density = cloud_shape.r;

	const float cloud_shape_erosion_fbm = saturate((cloud_shape.g * 0.625 + cloud_shape.b * 0.25 + cloud_shape.a * 0.125) + shape_base_erosion_bias);
	density = remap(density + shape_base_bias, -(1 - cloud_shape_erosion_fbm), 1.0, 0.0, 1.0f);

	const float cloud_type_base = 0.5;
	const float cloud_type = saturate(cloud_type_base + type_bias);
	density = apply_cloud_type(density, height_signal, cloud_type);

	float4 cloud_weather_data = cloud_weather_texture.Sample(default_sampler, sample_position_ws.xz * 0.00002 * (1 - weather_sample_scale_bias));

	const float coverage_base = cloud_weather_data.r;
	const float coverage = saturate(coverage_base + coverage_bias);
	density = apply_cloud_coverage(density, coverage);

	//const float cloud_detail_sample_scale_base = 0.0002f;
	//const float cloud_detail_sample_scale = (cloud_detail_sample_scale_base * (1 - detail_sample_scale_bias));
	//const float4 cloud_detail = cloud_detail_texture.Sample(default_sampler, sample_position_ws * cloud_detail_sample_scale);

	//const float cloud_detail_erosion_fbm = saturate((cloud_detail.r * 0.625 + cloud_detail.g * 0.25 + cloud_detail.b * 0.125) + shape_detail_erosion_bias);
	//density = remap(density + shape_detail_bias, 1 - cloud_detail_erosion_fbm, 1.0f, 0.0, 1.0);

	return saturate(density);
}

float hg(float cos_theta, float g)
{
	return (1.0 / 4.0 * PI) * ((1 - g * g) / pow(1 + g * g - 2 * g * cos_theta, 3.0 / 2.0));
}

float4 ps_main(ps_input input) : SV_Target0
{
	const float3 background = float3(0.53, 0.80, 0.92);

	const float depth = gbuffer_depth_texture.Sample(default_sampler, input.texcoord).r;

	const float3 position_ws = position_ws_from_depth(depth, input.texcoord, inverse_view_projection_matrix);

	const float3 direction_from_camera_ws = normalize(position_ws - camera_position_ws);

	// Look for ground
	float distance_from_camera_to_planet_surface_ws;
	if (intersect_ray_plane(float3(0.0f, -1.0f, 0.0f), float3(0.0, 0.0, 0.0), camera_position_ws, direction_from_camera_ws, distance_from_camera_to_planet_surface_ws))
	{
		return float4(0.0f, 0.8f, 0.0f, 1.0f);
	}

	float distance_from_camera_to_cloud_layer_height_begin_ws;
	if (!intersect_ray_plane(float3(0.0f, 1.0f, 0.0f), float3(0.0, cloud_layer_height_begin, 0.0), camera_position_ws, direction_from_camera_ws, distance_from_camera_to_cloud_layer_height_begin_ws))
	{
		return float4(1.0f, 0.0f, 0.0f, 1.0f);
	}

	if (distance_from_camera_to_cloud_layer_height_begin_ws < 0)
	{
		return float4(1.0f, 0.0f, 0.0f, 1.0f);
	}

	float distance_from_camera_to_cloud_layer_height_end_ws;
	if (!intersect_ray_plane(float3(0.0f, 1.0f, 0.0f), float3(0.0, cloud_layer_height_end, 0.0), camera_position_ws, direction_from_camera_ws, distance_from_camera_to_cloud_layer_height_end_ws))
	{
		return float4(1.0f, 0.0f, 0.0f, 1.0f);
	}

	if (distance_from_camera_to_cloud_layer_height_end_ws < 0)
	{
		return float4(1.0f, 0.0f, 0.0f, 1.0f);
	}

	if (distance_from_camera_to_cloud_layer_height_end_ws <= distance_from_camera_to_cloud_layer_height_begin_ws)
	{
		return float4(1.0f, 0.0f, 0.0f, 1.0f);
	}

	const float3 direction_to_light_ws = normalize(-sun_direction_ws);
	const float cos_theta = dot(direction_from_camera_ws, direction_to_light_ws);

	const int step_count_max = 128;
	const int shadow_step_count_max = 4;

	float3 scattering = float3(0.0, 0.0, 0.0);
	float transmittance = 1.0f;

	float sample_distance_ws = distance_from_camera_to_cloud_layer_height_begin_ws;

	bool raymarch_terminated = false;

#if defined(ENABLE_RAYMARCH_DEBUGGING)
	int debug_raymarch_termination_reason = 0;
#endif

	[loop]// [unroll(step_count_max)]
	for (int i = 0; i < step_count_max; ++i)
	{
		if (!raymarch_terminated)
		{
			const float3 sample_position_ws = camera_position_ws + direction_from_camera_ws * sample_distance_ws;
			const float sample_density = sample_cloud_density(sample_position_ws);
			if (sample_density > EPSILON)
			{
				bool shadow_raymarch_terminated = false;

#if defined(ENABLE_RAYMARCH_DEBUGGING)
				int debug_raymarch_shadow_termination_reason = 0;
#endif

				float shadow = 1.0;
				{
					float shadow_sample_distance_ws = shadow_step_size_ws;

					[loop] //[unroll(shadow_step_count_max)]
					for (int j = 0; j < shadow_step_count_max; ++j)
					{
						if (!shadow_raymarch_terminated)
						{
							const float3 shadow_sample_position_ws = sample_position_ws + direction_to_light_ws * shadow_sample_distance_ws;
							const float shadow_sample_density = sample_cloud_density(shadow_sample_position_ws);

							const float scattering_coeff = shadow_sample_density * scattering_factor;
							const float absorption_coeff = shadow_sample_density * absorption_factor;
							const float extinction_coeff = scattering_coeff + absorption_coeff;

							const float shadow_at_step = exp(-extinction_coeff * shadow_step_size_ws);

							shadow *= shadow_at_step;

							if (shadow < EPSILON)
							{
								shadow_raymarch_terminated = true;
#if defined(ENABLE_RAYMARCH_DEBUGGING)
								debug_raymarch_shadow_termination_reason = 1;
#endif
							}

							if (shadow_sample_position_ws.y > cloud_layer_height_end)
							{
								shadow_raymarch_terminated = true;

#if defined(ENABLE_RAYMARCH_DEBUGGING)
								debug_raymarch_shadow_termination_reason = 2;
#endif
							}

#if defined(ENABLE_RAYMARCH_DEBUGGING)
							if (j == shadow_step_count_max - 1)
							{
								shadow_raymarch_terminated = true;

								debug_raymarch_shadow_termination_reason = 3;
							}
#endif

							shadow_sample_distance_ws += shadow_step_size_ws;
						}
					}
				}

#if defined(ENABLE_RAYMARCH_DEBUGGING)
				if (debug_shadow_raymarch_termination && debug_shadow_raymarch_termination_step == i && shadow_raymarch_terminated)
				{
					raymarch_terminated = true;

					debug_raymarch_termination_reason = debug_raymarch_shadow_termination_reason;
				}
				else
#endif
				{
					const float phase = lerp(hg(cos_theta, 0.8), hg(cos_theta, -0.5), 0.5);
					const float3 light = sun_light.xxx * phase * shadow + ambient_light.xxx;

					const float scattering_coeff = sample_density * scattering_factor;
					const float absorption_coeff = sample_density * absorption_factor;
					const float extinction_coeff = scattering_coeff + absorption_coeff;

					const float3 scattering_at_step = light * scattering_coeff;
					const float transmittance_at_step = exp(-extinction_coeff * step_size_ws);

					if (debug_toggle_frostbite_scattering)
					{
						scattering += transmittance * ((scattering_at_step - scattering_at_step * transmittance_at_step) / extinction_coeff);
					}
					else
					{
						scattering += transmittance * scattering_at_step * step_size_ws;
					}

					transmittance *= transmittance_at_step;
				}
			}

			if (transmittance < EPSILON)
			{
				raymarch_terminated = true;

#if defined(ENABLE_RAYMARCH_DEBUGGING)
				debug_raymarch_termination_reason = 1;
#endif
			}

			if (sample_distance_ws >= distance_from_camera_to_cloud_layer_height_end_ws)
			{
				raymarch_terminated = true;

#if defined(ENABLE_RAYMARCH_DEBUGGING)
				debug_raymarch_termination_reason = 2;
#endif
			}

#if defined(ENABLE_RAYMARCH_DEBUGGING)
			if (i == step_count_max - 1)
			{
				raymarch_terminated = true;

				debug_raymarch_termination_reason = 3;
			}
#endif

			sample_distance_ws += step_size_ws;
		}
	}

#if defined(ENABLE_RAYMARCH_DEBUGGING)
	if (raymarch_terminated && (debug_raymarch_termination || debug_shadow_raymarch_termination))
	{
		switch (debug_raymarch_termination_reason)
		{
		case 1: return float4(0.0, 0.0, 1.0, 1.0);  // extinction
		case 2: return float4(0.0, 1.0, 0.0, 1.0);  // exited volume
		case 3: return float4(1.0, 0.0, 0.0, 1.0);  // max steps
		default: return float4(0.0, 0.0, 0.0, 1.0); // ??
		}
	}
#endif

	// Tonemap and then gamma correct
	scattering = tonemap(scattering);
	scattering = linear_to_srgb(scattering);

	return float4(background * transmittance + scattering, 1.0);
}

// https://graphics.pixar.com/library/ProductionVolumeRendering/paper.pdf
// https://www.guerrilla-games.com/read/the-real-time-volumetric-cloudscapes-of-horizon-zero-dawn
// https://www.guerrilla-games.com/read/nubis-authoring-real-time-volumetric-cloudscapes-with-the-decima-engine
// https://www.ea.com/frostbite/news/physically-based-sky-atmosphere-and-cloud-rendering
// https://patapom.com/topics/Revision2013/Revision%202013%20-%20Real-time%20Volumetric%20Rendering%20Course%20Notes.pdf
// https://cgl.ethz.ch/teaching/former/scivis_07/Notes/stuff/StuttgartCourse/VIS-Modules-06-Direct_Volume_Rendering.pdf
// https://www.addymotion.com/projects/code/clouds/
// http://www.cse.chalmers.se/~uffe/xjobb/RurikH%C3%B6gfeldt.pdf