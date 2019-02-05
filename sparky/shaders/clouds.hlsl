#include "per_frame_cbuffer.hlsli"
#include "fullscreen_triangle.hlsli"
#include "position_from_depth.hlsli"
#include "noise.hlsli"

Texture2D gbuffer_depth_texture : register(t0);
Texture3D low_freq_noise_texture : register(t1);
Texture3D high_freq_noise_texture : register(t2);

SamplerState default_sampler : register(s0);
SamplerState clouds_sampler : register(s1);

#define CLOUD_LAYER_HEIGHT_BEGIN 1500.0f
#define CLOUD_LAYER_HEIGHT_END   4000.0f
#define PLANET_RADIUS            100.0f

float remap(float original_value, float original_min, float original_max, float new_min, float new_max)
{
	return new_min + (((original_value - original_min) / (original_max - original_min)) * (new_max - new_min));
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

float get_normalized_height_in_cloud_layer(float3 position_ws)
{
	return saturate((position_ws.y - CLOUD_LAYER_HEIGHT_BEGIN) / (CLOUD_LAYER_HEIGHT_END - CLOUD_LAYER_HEIGHT_BEGIN));
}

float sample_cloud_density(float3 position_ws, float3 direction_to_light_ws)
{
	const float height_cl = get_normalized_height_in_cloud_layer(position_ws);

	return noise(position_ws) > 0.5 ? 1.0f : 0.0f * height_cl;
}

float4 ps_main(ps_input input) : SV_Target0
{
	const float depth = gbuffer_depth_texture.Sample(default_sampler, input.texcoord).r;

	const float3 position_ws = position_ws_from_depth(depth, input.texcoord, projection_matrix, inverse_view_projection_matrix);

	const float3 direction_from_camera_ws = normalize(position_ws - camera_position_ws);

	const float3 planet_center_ws = float3(0.0f, -PLANET_RADIUS, 0.0f);

	float t;

	if (intersect_ray_sphere(planet_center_ws, PLANET_RADIUS + CLOUD_LAYER_HEIGHT_BEGIN, camera_position_ws, direction_from_camera_ws, t0))
	{
		const float3 direction_to_light_ws = normalize(-sun_direction_ws);

		return sample_cloud_density(planet_center_ws + direction_from_camera_ws * t0, direction_to_light_ws);
	}

	return float4(0.0f, 1.0f, 0.0f, 1.0f);
}

// https://www.guerrilla-games.com/read/the-real-time-volumetric-cloudscapes-of-horizon-zero-dawn
// https://www.guerrilla-games.com/read/nubis-authoring-real-time-volumetric-cloudscapes-with-the-decima-engine
// https://www.ea.com/frostbite/news/physically-based-sky-atmosphere-and-cloud-rendering