#include "per_frame_cbuffer.hlsli"
#include "fullscreen_triangle.hlsli"
#include "position_from_depth.hlsli"
#include "tonemap.hlsli"
#include "gamma_correction.hlsli"

Texture2D gbuffer_base_color_texture : register(t0);
Texture2D gbuffer_metalness_roughness_texture : register(t1);
Texture2D gbuffer_normal_map_texture : register(t2);
Texture2D gbuffer_depth_texture : register(t3);

Texture2D environment_specular_texture : register(t4);

SamplerState default_sampler : register(s0);

static const float PI = 3.14159265f;
static const float EPSILON = 1e-10;

#include "brdf.hlsli"

float4x4 inverse(float4x4 input)
{
#define minor(a,b,c) determinant(float3x3(input.a, input.b, input.c))
	//determinant(float3x3(input._22_23_23, input._32_33_34, input._42_43_44))

	float4x4 cofactors = float4x4(
		minor(_22_23_24, _32_33_34, _42_43_44),
		-minor(_21_23_24, _31_33_34, _41_43_44),
		minor(_21_22_24, _31_32_34, _41_42_44),
		-minor(_21_22_23, _31_32_33, _41_42_43),

		-minor(_12_13_14, _32_33_34, _42_43_44),
		minor(_11_13_14, _31_33_34, _41_43_44),
		-minor(_11_12_14, _31_32_34, _41_42_44),
		minor(_11_12_13, _31_32_33, _41_42_43),

		minor(_12_13_14, _22_23_24, _42_43_44),
		-minor(_11_13_14, _21_23_24, _41_43_44),
		minor(_11_12_14, _21_22_24, _41_42_44),
		-minor(_11_12_13, _21_22_23, _41_42_43),

		-minor(_12_13_14, _22_23_24, _32_33_34),
		minor(_11_13_14, _21_23_24, _31_33_34),
		-minor(_11_12_14, _21_22_24, _31_32_34),
		minor(_11_12_13, _21_22_23, _31_32_33)
		);
#undef minor
	return transpose(cofactors) / determinant(input);
}

struct point_light
{
	float3 position_ws;
	float3 intensity;      // Radiant intensity. Power per unit steradian.
};

struct directional_light
{
	float3 direction_ws; 
	float3 irradiance;	// Power per unit area received by surface with normal facing direction
};

// https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
float3 importance_sample_ggx(float2 Xi, float alpha)
{
	// Returns samples in tangent space (Z)
	float alpha2 = alpha * alpha;
	float phi = 2.0 * PI * Xi.x;
	float cos_theta2 = (1.0 - Xi.y) / (1.0 + (alpha2 - 1.0) * Xi.y);
	float cos_theta = sqrt(cos_theta2);
	float sin_theta = sqrt(1.0 - cos_theta2);
	return float3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta); 
}

float3x3 orthonormal_basis(float3 n)
{
	float3 up = abs(n.z) < (1 - EPSILON) ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);

	float3 v = normalize(cross(up, n));
	float3 u = cross(n, v);

	return float3x3(v, u, n);
}

// http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
float2 hammersley(uint i, uint N)
{
	uint bits = i;
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float2(float(i) / float(N), float(bits) * 2.3283064365386963e-10);
}

float4 ps_main(ps_input input) : SV_Target0
{
	directional_light sun_light;
	sun_light.direction_ws = sun_direction_ws;
	sun_light.irradiance = 10;

	const float depth = gbuffer_depth_texture.Sample(default_sampler, input.texcoord).r;

	if (depth >= (1 - EPSILON))
	{
		return float4(0.0f, 0.0f, 0.0f, 1.0f);
	}

	const float3 base_color = gbuffer_base_color_texture.Sample(default_sampler, input.texcoord).xyz;
	const float2 metalness_roughness = gbuffer_metalness_roughness_texture.Sample(default_sampler, input.texcoord).rg;
	const float metalness = metalness_roughness.r;
	const float disney_roughness = max(metalness_roughness.g * metalness_roughness.g, 0.003);
	const float3 normal_ws = normalize(gbuffer_normal_map_texture.Sample(default_sampler, input.texcoord).xyz * 2 - 1);

	const float3 position_ws = position_ws_from_depth(depth, input.texcoord, inverse_view_projection_matrix);
	const float3 direction_to_camera_ws = normalize(camera_position_ws - position_ws.xyz);

	const float3 diffuse_color = base_color * (1.0f - metalness);
	const float3 specular_color = lerp(0.04f, base_color, metalness);

	const float n_dot_v = saturate(dot(normal_ws, direction_to_camera_ws));

	float3 direct_lighting = float3(0.0, 0.0, 0.0);
	{
		const float3 radiance = float3(1.0, 1.0, 1.0) * 10;

		const float3 direction_to_light_ws = normalize(-sun_light.direction_ws);
		const float3 half_vector_ws = normalize(direction_to_light_ws + direction_to_camera_ws);

		const float n_dot_l = saturate(dot(normal_ws, direction_to_light_ws));
		const float n_dot_h = saturate(dot(normal_ws, half_vector_ws));
		const float l_dot_h = saturate(dot(direction_to_light_ws, half_vector_ws));

		const float3 f_s = specular(specular_color, n_dot_v, n_dot_l, n_dot_h, l_dot_h, disney_roughness);
		const float3 f_d = diffuse(diffuse_color);

		direct_lighting += (f_s + f_d) * n_dot_l * radiance * PI;
	}

	float3 indirect_lighting = float3(0.0, 0.0, 0.0);
	{
		const float3x3 tangent_to_world = orthonormal_basis(normal_ws);

		const int sample_count = 64;

		for (int i = 0; i < sample_count; ++i)
		{
			const float2 x = hammersley(i, sample_count);
			const float3 half_vector_ts = importance_sample_ggx(x, disney_roughness);
			const float3 half_vector_ws = mul(half_vector_ts, tangent_to_world);

			float3 direction_to_light_ws = reflect(-direction_to_camera_ws, half_vector_ws);

			const float n_dot_l = saturate(dot(normal_ws, direction_to_light_ws));
			const float n_dot_h = dot(normal_ws, half_vector_ws);
			const float l_dot_h = saturate(dot(direction_to_light_ws, half_vector_ws));

			float ipdf = (4.0 * l_dot_h) / (distribution(n_dot_h, disney_roughness) * n_dot_h); // must be d_ggx

			// spherical mapping a.k.a equirectangular a.k.a latlong
			float phi = atan2(direction_to_light_ws.z, direction_to_light_ws.x);
			float theta = acos(direction_to_light_ws.y);
			float2 texcoord = float2(phi / (PI * 2), theta / PI);

			uint width, height, levels;
			environment_specular_texture.GetDimensions(0, width, height, levels);

			float3 light_color = environment_specular_texture.SampleLevel(default_sampler, texcoord, metalness * levels).rgb;

			indirect_lighting += specular(light_color, n_dot_v, n_dot_l, n_dot_h, l_dot_h, disney_roughness) * ipdf;
		}
	}

	float3 lighting = direct_lighting + indirect_lighting;

	// Tonemap and then gamma correct
	lighting = tonemap(lighting);
	lighting = linear_to_srgb(lighting);

	return float4(lighting, 1.0);
}

// https://disney-animation.s3.amazonaws.com/library/s2012_pbs_disney_brdf_notes_v2.pdf
// http://www.thetenthplanet.de/archives/255
// https://renderman.pixar.com/view/simple-shader-writing
// http://www.codinglabs.net/article_physically_based_rendering_cook_torrance.aspx
// http://blog.selfshadow.com/publications/s2015-shading-course/hoffman/s2015_pbs_physics_math_slides.pdf
// http://www.scratchapixel.com/old/lessons/3d-advanced-lessons/things-to-know-about-the-cg-lighting-pipeline/what-is-a-brdf/
// http://www.klayge.org/wiki/index.php/Physically-based_BRDF
// http://www.rorydriscoll.com/2009/01/25/energy-conservation-in-games/
// http://www.joshbarczak.com/blog/?p=272
// http://www.reedbeta.com/blog/2013/07/31/hows-the-ndf-really-defined/