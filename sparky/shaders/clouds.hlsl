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
Texture2D cloud_weather_texture : register(t3);

SamplerState default_sampler : register(s0);

#define PI      3.1415926f
#define EPSILON 0.000001f

#define PLANET_RADIUS            4000.0f

//#define ENABLE_RAYMARCH_DEBUGGING

//#define DEBUG_CLOUD_WEATHER_SPHERE
//#define DEBUG_CLOUD_WEATHER_CYLINDER
//#define DEBUG_CLOUD_WEATHER_GRID

#define DEBUG_PLANET_FLAT

float remap(float original_value, float original_min, float original_max, float new_min, float new_max)
{
	return new_min + (((original_value - original_min) / (original_max - original_min)) * (new_max - new_min));
}

bool intersect_ray_sphere(float3 sphere_center, float sphere_radius, float3 ray_origin, float3 ray_dir, out float t)
{
	const float3 L = sphere_center - ray_origin;
	const float t_ca = dot(L, ray_dir);
	const float d2 = dot(L, L) - t_ca * t_ca;
	const float r2 = sphere_radius * sphere_radius;

	if (d2 > r2)
	{
		t = 0.0;
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
			t = 0.0;
			return false;
		}
	}

	t = t_0;

	return true;
}

bool intersect_ray_plane(float3 plane_normal, float3 point_on_plane, float3 ray_origin, float3 ray_dir, out float t)
{
	float d = dot(ray_dir, plane_normal);

	if (d <= 0)
	{
		t = 0.0;
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

// The height signal ranges from 0 at the bottom of the cloud layer to 1 at the top
float cloud_layer_height_signal(float3 sample_position_ws, float3 planet_center_ws)
{
#if defined(DEBUG_PLANET_FLAT)
	const float sample_height = sample_position_ws.y;
#else
	const float sample_height = distance(planet_center_ws, sample_position_ws) - PLANET_RADIUS;
#endif
	return saturate((sample_height - cloud_layer_height_begin) / (cloud_layer_height_end - cloud_layer_height_begin));
}

float sample_cloud_density(float3 sample_position_ws, float height_signal)
{
	sample_position_ws += 100000.0f;

#if defined(DEBUG_CLOUD_WEATHER_CYLINDER)
	const float coverage_base = (distance(sample_position_ws.xz * (1 - weather_sample_scale_bias), float2(0.0, 0.0)) < 1000) ? 1.0 : 0.0;
#elif defined(DEBUG_CLOUD_WEATHER_SPHERE)
	const float coverage_base = (distance(sample_position_ws * (1 - weather_sample_scale_bias), float3(0.0, (cloud_layer_height_end - cloud_layer_height_begin) / 2.0, 0.0)) < 1000) ? 1.0 : 0.0;
#elif defined(DEBUG_CLOUD_WEATHER_GRID)
	const float coverage_base = abs((int)(sample_position_ws.x * 0.001 * (1 - weather_sample_scale_bias)) % 4) == 0 && abs((int)(sample_position_ws.z * 0.001 * (1 - weather_sample_scale_bias)) % 4) == 0 ? 1.0 : 0.0;
#else
	const float coverage_base = cloud_weather_texture.Sample(default_sampler, sample_position_ws.xz * 0.00002 * (1 - weather_sample_scale_bias)).r;
#endif

	const float cloud_shape_sample_scale_base = 0.0002f;
	const float cloud_shape_sample_scale = (cloud_shape_sample_scale_base * (1 - shape_sample_scale_bias));
	const float4 cloud_shape = cloud_shape_texture.Sample(default_sampler, sample_position_ws * cloud_shape_sample_scale);

	float density = cloud_shape.r;

	const float cloud_shape_erosion_fbm = saturate((cloud_shape.g * 0.625 + cloud_shape.b * 0.25 + cloud_shape.a * 0.125) + shape_base_erosion_bias);
	density = remap(density + shape_base_bias, -(1 - cloud_shape_erosion_fbm), 1.0, 0.0, 1.0f);

	const float cloud_type_base = 0.5;
	const float cloud_type = saturate(cloud_type_base + type_bias);
	density = apply_cloud_type(density, height_signal, cloud_type);

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
			const float sample_height_signal = cloud_layer_height_signal(sample_position_ws, planet_center_ws);

			const float sample_density = sample_cloud_density(sample_position_ws, sample_height_signal);
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
							const float shadow_sample_height_signal = cloud_layer_height_signal(shadow_sample_position_ws, planet_center_ws);

							const float shadow_sample_density = sample_cloud_density(shadow_sample_position_ws, shadow_sample_height_signal);

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

#if defined(DEBUG_PLANET_FLAT)
							if (shadow_sample_position_ws.y > cloud_layer_height_end)
#else
#error "Need to cast ray from sample through cloud layer in direction of sun"
#endif
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

	return float4(background * transmittance + tonemap(scattering), 1.0);
}

// https://graphics.pixar.com/library/ProductionVolumeRendering/paper.pdf
// https://www.guerrilla-games.com/read/the-real-time-volumetric-cloudscapes-of-horizon-zero-dawn
// https://www.guerrilla-games.com/read/nubis-authoring-real-time-volumetric-cloudscapes-with-the-decima-engine
// https://www.ea.com/frostbite/news/physically-based-sky-atmosphere-and-cloud-rendering
// https://patapom.com/topics/Revision2013/Revision%202013%20-%20Real-time%20Volumetric%20Rendering%20Course%20Notes.pdf
// https://cgl.ethz.ch/teaching/former/scivis_07/Notes/stuff/StuttgartCourse/VIS-Modules-06-Direct_Volume_Rendering.pdf
// https://www.addymotion.com/projects/code/clouds/
// http://www.cse.chalmers.se/~uffe/xjobb/RurikH%C3%B6gfeldt.pdf

/*
Production Volume Rendering (SIGGRAPH 2017) Notes

--------------------------------------------------------------------------------------
Symbol                 Description
--------------------------------------------------------------------------------------
x                      Position
t                      Ray parameter
omega                  Ray direction
x_t                    Parameterized position along a ray: x_t = x + t * omega
--------------------------------------------------------------------------------------
sigma_a(x)             Absorption coefficient
sigma_s(x)             Scattering coefficient
sigma_t(x)             Extinction coefficient: sigma_t(x) = sigma_a(x) + sigma_s(x)
alpha(x)               Single scattering albedo: alpha(x) = sigma_s(x) / sigma_t(x)
f_p(x, omega, omega')  Phase function
--------------------------------------------------------------------------------------
d                      Ray length/domain of volume integration: 0 < t < d
xi, zeta               Random numbers
L(x, omega)            Radiance at x in direction omega
L_d(x_d, omega)        Incident boundary radiance at end of ray
L_e(x, omega)          Emitted radiance
L_s(x, omega)          In-scattered radiance at x from direction omega
--------------------------------------------------------------------------------------

2 VOLUME RENDERING THEORY

2.1 Properties of Volumes

The chance of a photon collision is defined by a coefficient sigma(x), the probability
density of collision per unit distance traveled inside the volume. The unit of a
collision coefficient is inverse length. Alternatively, can be represented as
"mean free path", the average distance traveled between collisions.

Absorption:
The absorption coefficient sigma_a(x) describes a collision where the photon i
absorbed by the volume (e.g. converted to heat).

Scattering:
The scattering coefficient sigma_s(x) describes a collision where the photon is
scattered in a different direction defined by the phase function. The radiance of the
photon is unchanged. Any change in radiance is modeled with absorption (or emission).

Phase function:
The phase function f_p(x, omega, omega') is the angular distribution of radiance
scattered. Usually modeled as a 1D function of the angle theta between the two
directions.  To be energy conserving phase functions needs to be normalized over the
sphere:

	int_{S^2} f_p(x, omega, omega') dtheta = 1

An isotropic volume has an equal probability of scattering incoming light in any
direction and has the associated phase function:

	f_p(x, theta) = 1 / 4pi

Phase functions are usually modeled with the Henyey-Greenstein phase function:

	f_p(x, theta) = (1 / 4pi) * ((1 - g^2) / (1 + g^2 - 2 * g * cos(theta))^(3/2))

The single parameter g [-1..1] can be understood as the average cosine of the
scattering direction and controls the asymmetry of the phase function. It can model
backwards scattering (g < 0), isotropic scattering (g = 0), and forward
scattering (g > 0). It can be perfectly importance sampled.

2.1.1 Extinction and Single Scattering Parametrization

In many cases it is desriable to choose a different parameterization than absorption
and scattering coefficients. We can define the extinction coeffficient
sigma_t = sigma_a + sigma_s (sometimes called attenuation cofficient or simply density).
In addition we define the single scattering albedo alpha(x) = sigma_s(x) / sigma_t(x).
An alpha(x) = 0 means all radiance is absorbed (such as black coal dust) and alpha(x) = 1
means no absorption occurs and we have lossless scattering (such as clouds).

2.4 PDF APPROACH

	L(x, omega) = int_{t=0}{d}T(t) sigma_s(x_t)L_s(x_s, omega) dt + T(d)L_d(x_d, omega)

The transmittance term T(t)

	T(t) = exp(-/int_{s=0}{t} sigma_t(x_s) ds)

is the transmittance from the origin x of the ray to the position x_t = x - t * omega
on the ray parameterized by t. The transmittance T(t) is the net reduction factor from
absorption and out-scattering between x and x_t.

2.4.1 Transmittance Estimators

Ray Marching Transmittance Estimator:
The transmittance can be estimated with quadrature instad of Monte Carlo, marching along
the ray and accumulating the transmittance. This introduces bias even if he marching is
jittered resulting in artifacts if the volume structure is undersampled.
*/
