#include "per_frame_cbuffer.hlsli"
#include "fullscreen_triangle.hlsli"
#include "position_from_depth.hlsli"
#include "noise.hlsli"
#include "tonemap.hlsli"
#include "gamma_correction.hlsli"
#include "clouds.cbuffer.hlsli"

Texture2D gbuffer_depth_texture : register(t0);
Texture3D cloud_shape_texture : register(t1);
Texture3D cloud_detail_texture : register(t2);
Texture2D weather_texture : register(t3);

SamplerState default_sampler : register(s0);

cbuffer constant_buffer_clouds_per_frame : register(b1)
{
	constant_buffer_clouds_per_frame_data clouds;
};

#define PI      3.1415926f
#define EPSILON 0.000001f

#define PLANET_RADIUS            4000.0f
#define CLOUD_LAYER_HEIGHT_BEGIN 1500.0f
#define CLOUD_LAYER_HEIGHT_END   4000.0f

//#define DEBUG_CLOUD_WEATHER_SPHERE
//#define DEBUG_CLOUD_WEATHER_CYLINDER
//#define DEBUG_CLOUD_WEATHER_GRID

#define DEBUG_PLANET_FLAT

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

bool intersect_ray_plane(float3 plane_normal, float3 point_on_plane, float3 ray_origin, float3 ray_dir, inout float t)
{
	float d = dot(ray_dir, plane_normal);

	if (d <= 0)
	{
		return false;
	}

	t = dot(point_on_plane - ray_origin, plane_normal) / d;

	return true;
}

// cloud type controls the the height of the cloud which in turn defines how billowy vs how wispy it is
float apply_cloud_type(float density, float height_cl, float cloud_type) 
{
	const float cumulus = saturate(remap(height_cl, 0.0, 0.2, 0.0, 1.0) * remap(height_cl, 0.7, 0.9, 1.0, 0.0));
	const float stratocumulus = saturate(remap(height_cl, 0.0, 0.2, 0.0, 1.0) * remap(height_cl, 0.2, 0.7, 1.0, 0.0));
	const float stratus = saturate(remap(height_cl, 0.0, 0.1, 0.0, 1.0) * remap(height_cl, 0.2, 0.3, 1.0, 0.0));

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

float get_normalized_height_in_cloud_layer(float3 planet_center_ws, float3 sample_position_ws)
{
#if defined(DEBUG_PLANET_FLAT)
	const float sample_height = sample_position_ws.y;
#else
	const float sample_height = distance(planet_center_ws, sample_position_ws) - PLANET_RADIUS;
#endif
	return saturate((sample_height - CLOUD_LAYER_HEIGHT_BEGIN) / (CLOUD_LAYER_HEIGHT_END - CLOUD_LAYER_HEIGHT_BEGIN));
}

float sample_cloud_density(float3 planet_center_ws, float3 sample_position_ws)
{
	sample_position_ws += 100000.0f;

#if defined(DEBUG_CLOUD_WEATHER_CYLINDER)
	float coverage_base = (distance(sample_position_ws.xz * (1 - clouds.weather_sample_scale_bias), float2(0.0, 0.0)) < 1000) ? 1.0 : 0.0;
#elif defined(DEBUG_CLOUD_WEATHER_SPHERE)
	float coverage_base = (distance(sample_position_ws * (1 - clouds.weather_sample_scale_bias), float3(0.0, (CLOUD_LAYER_HEIGHT_END - CLOUD_LAYER_HEIGHT_BEGIN) / 2.0, 0.0)) < 1000) ? 1.0 : 0.0;
#elif defined(DEBUG_CLOUD_WEATHER_GRID)
	float coverage_base = abs((int)(sample_position_ws.x * 0.001 * (1 - clouds.weather_sample_scale_bias)) % 4) == 0 && abs((int)(sample_position_ws.z * 0.001 * (1 - weather_sample_scale_bias)) % 4) == 0 ? 1.0 : 0.0;
#else
	float coverage_base = weather_texture.Sample(default_sampler, sample_position_ws.xz * 0.0002 * (1 - clouds.weather_sample_scale_bias)).r;
#endif

	//const float cloud_detail_sample_scale_base = 0.0002f;
	//float4 cloud_detail = cloud_detail_texture.Sample(default_sampler, sample_position_ws * (cloud_detail_sample_scale_base * (1 - detail_sample_scale_bias)));

	const float cloud_shape_sample_scale_base = 0.0002f;

	float4 cloud_shape = cloud_shape_texture.Sample(default_sampler, sample_position_ws * (cloud_shape_sample_scale_base * (1 - clouds.shape_sample_scale_bias)));

	float density = remap(cloud_shape.r + clouds.shape_base_bias, -(1 - (0.625 * cloud_shape.g + 0.25 * cloud_shape.b + 0.125 * cloud_shape.a + clouds.shape_detail_bias)), 1.0, 0.0, 1.0f);

	density = remap(density, 1 - coverage_base, 1.0, 0.0, 1.0);
	density *= coverage_base;
	
	density = saturate(density);

	return density;

	float coverage = saturate(coverage_base + clouds.coverage_bias);

	density = apply_cloud_coverage(density, coverage);

	const float height_cl = get_normalized_height_in_cloud_layer(planet_center_ws, sample_position_ws);

	float cloud_type_base = 0.5;
	float cloud_type = saturate(cloud_type_base + clouds.type_bias);

	density = apply_cloud_type(density, height_cl, cloud_type);

	return saturate(1 - density);
}

float4 ps_main(ps_input input) : SV_Target0
{
	const float depth = gbuffer_depth_texture.Sample(default_sampler, input.texcoord).r;

	const float3 position_ws = position_ws_from_depth(depth, input.texcoord, projection_matrix, inverse_view_projection_matrix);

	const float3 direction_from_camera_ws = normalize(position_ws - camera_position_ws);

#if defined(DEBUG_PLANET_FLAT)
	const float3 planet_center_ws = float3(position_ws.x, -PLANET_RADIUS, position_ws.z);

	// Look for ground
	float distance_from_camera_to_planet_surface_ws;
	if (intersect_ray_plane(float3(0.0f, -1.0f, 0.0f), float3(0.0, 0.0, 0.0), camera_position_ws, direction_from_camera_ws, distance_from_camera_to_planet_surface_ws))
	{
		return float4(0.0f, 0.8f, 0.0f, 1.0f);
	}

	float distance_from_camera_to_cloud_layer_height_begin_ws;
	if (!intersect_ray_plane(float3(0.0f, 1.0f, 0.0f), float3(0.0, CLOUD_LAYER_HEIGHT_BEGIN, 0.0), camera_position_ws, direction_from_camera_ws, distance_from_camera_to_cloud_layer_height_begin_ws))
	{
		return float4(1.0f, 0.0f, 0.0f, 1.0f);
	}

	float distance_from_camera_to_cloud_layer_height_end_ws;
	if (!intersect_ray_plane(float3(0.0f, 1.0f, 0.0f), float3(0.0, CLOUD_LAYER_HEIGHT_END, 0.0), camera_position_ws, direction_from_camera_ws, distance_from_camera_to_cloud_layer_height_end_ws))
	{
		return float4(1.0f, 0.0f, 0.0f, 1.0f);
	};
#else
	// Position planet such that surface is at origin
	const float3 planet_center_ws = float3(0.0f, -PLANET_RADIUS, 0.0f);

	// Look for ground
	float distance_from_camera_to_planet_surface_ws;
	if (intersect_ray_sphere(planet_center_ws, PLANET_RADIUS, camera_position_ws, direction_from_camera_ws, distance_from_camera_to_planet_surface_ws))
	{
		return float4(0.0f, 0.8f, 0.0f, 1.0f);
	}

	float distance_from_camera_to_cloud_layer_height_begin_ws;
	if (!intersect_ray_sphere(planet_center_ws, PLANET_RADIUS + CLOUD_LAYER_HEIGHT_BEGIN, camera_position_ws, direction_from_camera_ws, distance_from_camera_to_cloud_layer_height_begin_ws))
	{
		return float4(1.0f, 0.0f, 0.0f, 1.0f);
	}

	float distance_from_camera_to_cloud_layer_height_end_ws;
	if (!intersect_ray_sphere(planet_center_ws, PLANET_RADIUS + CLOUD_LAYER_HEIGHT_END, camera_position_ws, direction_from_camera_ws, distance_from_camera_to_cloud_layer_height_end_ws))
	{
		return float4(1.0f, 0.0f, 0.0f, 1.0f);
	};
#endif

	// const float3 direction_to_light_ws = normalize(-sun_direction_ws);

	// TODO: Should the size of each step depend on optical distance? Looking straight up through atmosphere will get the same
	// number of samples as looking into the horizon.
	int step_count = 128;

	const float inverse_step_count = 1.0f / step_count;

	float step_size_ws = (distance_from_camera_to_cloud_layer_height_end_ws - distance_from_camera_to_cloud_layer_height_begin_ws) / step_count;

	float3 scattering = float3(0.0, 0.0, 0.0);
	float extinction = 1.0f;

	for (int i = 0; i < step_count; ++i)
	{
		float distance_from_camera_to_cloud_layer_sample_position_ws = distance_from_camera_to_cloud_layer_height_begin_ws + (step_size_ws * i);

		float3 sample_position_ws = camera_position_ws + direction_from_camera_ws * distance_from_camera_to_cloud_layer_sample_position_ws;

		float density = sample_cloud_density(planet_center_ws, sample_position_ws);

		extinction *= exp(-density * inverse_step_count * (1.0 + clouds.extinction_coeff_bias));

		scattering += extinction * density * (1.0 + clouds.scattering_coeff_bias) * inverse_step_count;
	}

	float3 sky = float3(0.53, 0.80, 0.92);
	return float4(sky * extinction + scattering, 1.0);
}

// https://www.guerrilla-games.com/read/the-real-time-volumetric-cloudscapes-of-horizon-zero-dawn
// https://www.guerrilla-games.com/read/nubis-authoring-real-time-volumetric-cloudscapes-with-the-decima-engine
// https://www.ea.com/frostbite/news/physically-based-sky-atmosphere-and-cloud-rendering
// https://patapom.com/topics/Revision2013/Revision%202013%20-%20Real-time%20Volumetric%20Rendering%20Course%20Notes.pdf