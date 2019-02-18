#include "per_frame_cbuffer.hlsli"
#include "fullscreen_triangle.hlsli"
#include "position_from_depth.hlsli"
#include "noise.hlsli"

Texture2D gbuffer_depth_texture : register(t0);
Texture3D cloud_shape_texture : register(t1);
Texture3D high_freq_noise_texture : register(t2);

SamplerState default_sampler : register(s0);
SamplerState clouds_sampler : register(s1);

#define PLANET_RADIUS            4000.0f
#define CLOUD_LAYER_HEIGHT_BEGIN 1500.0f
#define CLOUD_LAYER_HEIGHT_END   4000.0f

float remap(float original_value, float original_min, float original_max, float new_min, float new_max)
{
	return new_min + (((clamp(original_value, original_min, original_max) - original_min) / (original_max - original_min)) * (new_max - new_min));
}

bool intersect_ray_sphere(float3 sphere_center, float sphere_radius, float3 ray_origin, float3 ray_dir, inout float t)
{
	const float3 L = sphere_center - ray_origin;
	const float t_ca = dot(L, ray_dir);
	const float d2 = dot(L, L) - t_ca * t_ca;
	const float r2 = sphere_radius * sphere_radius;

	if (d2 > r2)
	{
		return false;
	}

	const float t_hc = sqrt(r2 - d2);

	float t_0 = t_ca - t_hc;
	float t_1 = t_ca + t_hc;

	if (t_0 > t_1)
	{
		const float temp = t_1;
		t_0 = t_1;
		t_1 = temp;
	}

	if (t_0 < 0)
	{
		t_0 = t_1;

		if (t_0 < 0)
		{
			return false;
		}
	}

	t = t_0;

	return true;
}

float get_cloud_type_factor(float height_cl, float cloud_type) 
{
	float cumulus = remap(height_cl, 0.0, 0.2, 0.0, 1.0) * remap(height_cl, 0.7, 0.9, 1.0, 0.0);
	float stratocumulus = remap(height_cl, 0.0, 0.2, 0.0, 1.0) * remap(height_cl, 0.2, 0.7, 1.0, 0.0);
	float stratus = remap(height_cl, 0.0, 0.1, 0.0, 1.0) * remap(height_cl, 0.2, 0.3, 1.0, 0.0);

	float d1 = lerp(stratus, stratocumulus, saturate(cloud_type * 2.0));
	float d2 = lerp(stratocumulus, cumulus, saturate((cloud_type - 0.5) * 2.0));
	return lerp(d1, d2, cloud_type);
}

float get_cloud_coverage_factor(float height_cl, float cloud_coverage)
{
	float anvil_bias = 0.5f;
	return pow(cloud_coverage, remap(height_cl, 0.7, 0.8, 1.0, lerp(1.0, 0.5, anvil_bias)));
}

float get_normalized_height_in_cloud_layer(float3 planet_center_ws, float3 sample_position_ws)
{
	const float sample_height = distance(planet_center_ws, sample_position_ws) - PLANET_RADIUS;

	return saturate((sample_height - CLOUD_LAYER_HEIGHT_BEGIN) / (CLOUD_LAYER_HEIGHT_END - CLOUD_LAYER_HEIGHT_BEGIN));
}

float sample_cloud_density(float3 planet_center_ws, float3 sample_position_ws, float3 direction_to_light_ws)
{
	const float cloud_shape_sample_scale = 0.0002f;

	const float3 sample_position_uv = sample_position_ws * cloud_shape_sample_scale;

#define DEBUG_CLOUD_DENSITY
#ifdef DEBUG_CLOUD_DENSITY
	float density = 0.0f;

	if (distance(sample_position_uv.xz, float2(0.0f, 0.0f)) < 0.1)
	{
		density = 0.05;
	}
#else
	const float4 cloud_shape = cloud_shape_texture.Sample(default_sampler, sample_position_uv);

	float density = cloud_shape.r;

	const float erosion = 0.625 * cloud_shape.y + 0.25 * cloud_shape.z + 0.125 * cloud_shape.w;

	density = saturate(remap(density, erosion, 1.0, 0.0, 1.0));
#endif

	const float height_cl = get_normalized_height_in_cloud_layer(planet_center_ws, sample_position_ws);

	float cloud_type = 0.0f;

	const float cloud_type_factor = get_cloud_type_factor(height_cl, cloud_type);

	float cloud_coverage = 0.5f;

	const float cloud_coverage_factor = get_cloud_coverage_factor(height_cl, cloud_coverage);

	return density * cloud_type_factor * cloud_coverage_factor;
}

float4 ps_main(ps_input input) : SV_Target0
{
	const float depth = gbuffer_depth_texture.Sample(default_sampler, input.texcoord).r;

	const float3 position_ws = position_ws_from_depth(depth, input.texcoord, projection_matrix, inverse_view_projection_matrix);

	const float3 direction_from_camera_ws = normalize(position_ws - camera_position_ws);

	const float3 planet_center_ws = float3(0.0f, -PLANET_RADIUS, 0.0f);

	// Look for ground
	float distance_from_camera_to_planet_surface_ws;
	if (intersect_ray_sphere(planet_center_ws, PLANET_RADIUS, camera_position_ws, direction_from_camera_ws, distance_from_camera_to_planet_surface_ws))
	{
		return float4(0.0f, 0.8f, 0.0f, 1.0f);
	}

	float distance_from_camera_to_cloud_layer_height_begin_ws;
	if (intersect_ray_sphere(planet_center_ws, PLANET_RADIUS + CLOUD_LAYER_HEIGHT_BEGIN, camera_position_ws, direction_from_camera_ws, distance_from_camera_to_cloud_layer_height_begin_ws))
	{
		float distance_from_camera_to_cloud_layer_height_end_ws;
		intersect_ray_sphere(planet_center_ws, PLANET_RADIUS + CLOUD_LAYER_HEIGHT_END, camera_position_ws, direction_from_camera_ws, distance_from_camera_to_cloud_layer_height_end_ws);

		const float3 direction_to_light_ws = normalize(-sun_direction_ws);

		int step_count = 64;

		float step_size_ws = (distance_from_camera_to_cloud_layer_height_end_ws - distance_from_camera_to_cloud_layer_height_begin_ws) / step_count;

		float density = 0.0f;

		for (int i = 0; i < step_count; ++i)
		{
			float distance_from_camera_to_cloud_layer_sample_position_ws = distance_from_camera_to_cloud_layer_height_begin_ws + (step_size_ws * i);

			float3 sample_position_ws = camera_position_ws + direction_from_camera_ws * distance_from_camera_to_cloud_layer_sample_position_ws;

			density += sample_cloud_density(planet_center_ws, sample_position_ws, direction_to_light_ws);
		}

		float3 sky_blue = float3(0.53, 0.80, 0.92);

		return float4(sky_blue + density, 1.0f);
	}

	return float4(0.0f, 1.0f, 0.0f, 1.0f);
}

// https://www.guerrilla-games.com/read/the-real-time-volumetric-cloudscapes-of-horizon-zero-dawn
// https://www.guerrilla-games.com/read/nubis-authoring-real-time-volumetric-cloudscapes-with-the-decima-engine
// https://www.ea.com/frostbite/news/physically-based-sky-atmosphere-and-cloud-rendering