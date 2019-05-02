#include "per_frame_cbuffer.hlsli"
#include "fullscreen_triangle.hlsli"
#include "position_from_depth.hlsli"
#include "noise.hlsli"

Texture2D gbuffer_depth_texture : register(t0);
Texture3D cloud_shape_texture : register(t1);
Texture3D cloud_detail_texture : register(t2);

SamplerState default_sampler : register(s0);

cbuffer clouds_per_frame_cbuffer : register(b1)
{
	float shape_sample_scale_bias;
	float detail_sample_scale_bias;
	float density_bias;
	float coverage_bias;
	float type_bias;
	float shape_base_bias;
	float shape_detail_bias;
	float debug0;
	float debug1;
	float debug2;
	float debug3;
	float debug4;
	float debug5;
	float debug6;
};

#define PI      3.1415926f
#define EPSILON 0.000001f

#define PLANET_RADIUS            4000.0f
#define CLOUD_LAYER_HEIGHT_BEGIN 1500.0f
#define CLOUD_LAYER_HEIGHT_END   4000.0f

//#define DEBUG_CLOUD_SHAPE_SPHERE
#define DEBUG_CLOUD_SHAPE_CYLINDER

#define DEBUG_PLANET_FLAT

float remap(float original_value, float original_min, float original_max, float new_min, float new_max)
{
	return new_min + (((original_value - original_min) / (original_max - original_min)) * (new_max - new_min));
}

float remap_and_clamp(float original_value, float original_min, float original_max, float new_min, float new_max)
{
	return clamp(new_min + (((original_value - original_min) / (original_max - original_min)) * (new_max - new_min)), new_min, new_max);
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

bool intersect_cloud_layer(float3 ray_origin, float3 ray_dir)
{

}

// cloud type controls the the height of the cloud which in turn defines how billowy vs how wispy it is
float apply_cloud_type(float density, float height_cl, float cloud_type) 
{
	const float cumulus = remap(height_cl, 0.0, 0.2, 0.0, 1.0) * remap(height_cl, 0.7, 0.9, 1.0, 0.0);
	const float stratocumulus = remap(height_cl, 0.0, 0.2, 0.0, 1.0) * remap(height_cl, 0.2, 0.7, 1.0, 0.0);
	const float stratus = remap(height_cl, 0.0, 0.1, 0.0, 1.0) * remap(height_cl, 0.2, 0.3, 1.0, 0.0);

	const float stratus_to_stratocumulus = lerp(stratus, stratocumulus, saturate(cloud_type * 2.0));
	const float stratocumulus_to_cumulus = lerp(stratocumulus, cumulus, saturate((cloud_type - 0.5) * 2.0));

	return density * lerp(stratus_to_stratocumulus, stratocumulus_to_cumulus, cloud_type);
}

// cloud coverage decides the presence of a cloud
float apply_cloud_coverage(float density, float height_cl, float cloud_coverage)
{
	// cloud_coverage = pow(cloud_coverage, remap_and_clamp(height_cl, 0.7, 0.8, 1.0, 0.5));

	density = remap(density, min(1 - cloud_coverage, 1.0 - EPSILON), 1.0, 0.0, 1.0);

	density *= cloud_coverage;

	return density;
}

float get_normalized_height_in_cloud_layer(float3 planet_center_ws, float3 sample_position_ws)
{
	const float sample_height = distance(planet_center_ws, sample_position_ws) - PLANET_RADIUS;

	return saturate((sample_height - CLOUD_LAYER_HEIGHT_BEGIN) / (CLOUD_LAYER_HEIGHT_END - CLOUD_LAYER_HEIGHT_BEGIN));
}

float sample_cloud_density(float3 planet_center_ws, float3 sample_position_ws)
{
#if defined(DEBUG_CLOUD_SHAPE_CYLINDER)
	float density = (distance(sample_position_ws.xz, float2(0.0, 0.0)) < 1000) ? 1.0 : 0.0;
#elif defined(DEBUG_CLOUD_SHAPE_SPHERE)
	float density = (distance(sample_position_ws, float3(0.0, (CLOUD_LAYER_HEIGHT_END - CLOUD_LAYER_HEIGHT_BEGIN) / 2.0, 0.0)) < 1000) ? 1.0 : 0.0;
#else
	const float cloud_shape_sample_scale_base = 0.0002f;
	const float cloud_detail_sample_scale_base = 0.0002f;

	float4 cloud_shape = cloud_shape_texture.Sample(default_sampler, sample_position_ws * (cloud_shape_sample_scale_base * (1 - shape_sample_scale_bias))) + shape_base_bias;
	float4 cloud_detail = cloud_detail_texture.Sample(default_sampler, sample_position_ws * (cloud_detail_sample_scale_base * (1 - detail_sample_scale_bias))) + shape_detail_bias;

	float density = remap(cloud_shape.r, cloud_detail.r, 1.0, 0.0, 1.0f) + density_bias;
	
	density = clamp(density, 0.0, 1.0f);

	// const float detail_fbm = 0.625 * cloud_shape.g + 0.25 * cloud_shape.b + 0.125 * cloud_shape.a;

	// float density = remap(cloud_shape.r * (1 + shape_base_bias), -(1 - detail_fbm * (1 + shape_detail_bias)), 1.0, 0.0, 1.0);
#endif

	return density;

	const float height_cl = get_normalized_height_in_cloud_layer(planet_center_ws, sample_position_ws);

	float cloud_type_base = 0.5;
	float cloud_type = saturate(cloud_type_base + type_bias);

	density = apply_cloud_type(density, height_cl, cloud_type);

	float coverage_base = 0.5;
	float cloud_coverage = saturate(coverage_base + coverage_bias);

	density = apply_cloud_coverage(density, height_cl, cloud_coverage);

	return saturate(density);

	//return density > 0.5 ? 1 : 0;
}

float4 ps_main(ps_input input) : SV_Target0
{
	const float depth = gbuffer_depth_texture.Sample(default_sampler, input.texcoord).r;

	const float3 position_ws = position_ws_from_depth(depth, input.texcoord, projection_matrix, inverse_view_projection_matrix);

	const float3 direction_from_camera_ws = normalize(position_ws - camera_position_ws);

	// Position planet such that surface is at origin
	const float3 planet_center_ws = float3(0.0f, -PLANET_RADIUS, 0.0f);

#if defined(DEBUG_PLANET_FLAT)
	// Look for ground
	float distance_from_camera_to_planet_surface_ws;
	if (intersect_ray_plane(float3(0.0f, -1.0f, 0.0f), float3(0.0f, 0.0, 0.0f), camera_position_ws, direction_from_camera_ws, distance_from_camera_to_planet_surface_ws))
	{
		return float4(0.0f, 0.8f, 0.0f, 1.0f);
	}

	float distance_from_camera_to_cloud_layer_height_begin_ws;
	if (!intersect_ray_plane(float3(0.0f, 1.0f, 0.0f), float3(0.0f, CLOUD_LAYER_HEIGHT_BEGIN, 0.0f), camera_position_ws, direction_from_camera_ws, distance_from_camera_to_cloud_layer_height_begin_ws))
	{
		return float4(1.0f, 0.0f, 0.0f, 1.0f);
	}

	float distance_from_camera_to_cloud_layer_height_end_ws;
	if (!intersect_ray_plane(float3(0.0f, 1.0f, 0.0f), float3(0.0f, CLOUD_LAYER_HEIGHT_END, 0.0f), camera_position_ws, direction_from_camera_ws, distance_from_camera_to_cloud_layer_height_end_ws))
	{
		return float4(1.0f, 0.0f, 0.0f, 1.0f);
	};
#else
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
	//if (debug3 > -0.9)
	//{
	//	step_count *= 2;
	//}
	//if (debug3 > -0.5)
	//{
	//	step_count *= 2;
	//}
	//if (debug3 > 0)
	//{
	//	step_count *= 2;
	//}
	//if (debug3 > 0.5)
	//{
	//	step_count *= 2;
	//}
	//if (debug3 > 0.9)
	//{
	//	step_count *= 2;
	//}

	const float inverse_step_count = 1.0f / step_count;

	float step_size_ws = (distance_from_camera_to_cloud_layer_height_end_ws - distance_from_camera_to_cloud_layer_height_begin_ws) / step_count;

	float extinction_coeff = 0.03f; // sigma_t (m^-1)
	float scattering_coeff = 0.01f; // sigma_s (m^-1)

	float3 scattering = float3(0.53, 0.80, 0.92);
	float extinction = 1.0f;

	for (int i = 0; i < step_count; ++i)
	{
		float distance_from_camera_to_cloud_layer_sample_position_ws = distance_from_camera_to_cloud_layer_height_begin_ws + (step_size_ws * i);

		float3 sample_position_ws = camera_position_ws + direction_from_camera_ws * distance_from_camera_to_cloud_layer_sample_position_ws;

		float density = sample_cloud_density(planet_center_ws, sample_position_ws);

		extinction *= exp(-density * step_size_ws * extinction_coeff);

		float luminance = 1.0;

		scattering += extinction * luminance * scattering_coeff * step_size_ws;
	}

	//extinction = 1.0f;

	float3 sky = float3(0.53, 0.80, 0.92);
	return float4(scattering, extinction);
}

// https://www.guerrilla-games.com/read/the-real-time-volumetric-cloudscapes-of-horizon-zero-dawn
// https://www.guerrilla-games.com/read/nubis-authoring-real-time-volumetric-cloudscapes-with-the-decima-engine
// https://www.ea.com/frostbite/news/physically-based-sky-atmosphere-and-cloud-rendering
// https://patapom.com/topics/Revision2013/Revision%202013%20-%20Real-time%20Volumetric%20Rendering%20Course%20Notes.pdf