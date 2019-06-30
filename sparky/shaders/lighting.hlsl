#include "per_frame_cbuffer.hlsli"
#include "fullscreen_triangle.hlsli"
#include "position_from_depth.hlsli"
#include "tonemap.hlsli"
#include "gamma_correction.hlsli"

Texture2D gbuffer_base_color_texture : register(t0);
Texture2D gbuffer_metalness_roughness_texture : register(t1);
Texture2D gbuffer_normal_map_texture : register(t2);
Texture2D gbuffer_depth_texture : register(t3);

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

	float3 indirect_lighting = float3(0.0, 0.0, 0.0);

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

	float3 lighting = direct_lighting + indirect_lighting;

	// Tonemap and then gamma correct
	lighting = tonemap(lighting);
	lighting = linear_to_srgb(lighting);

	return float4(lighting, 1.0);
}