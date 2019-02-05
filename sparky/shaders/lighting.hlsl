#include "per_frame_cbuffer.hlsli"
#include "fullscreen_triangle.hlsli"
#include "position_from_depth.hlsli"

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

// https://kosmonautblog.wordpress.com/2017/03/26/designing-a-linear-hdr-pipeline/
// http://renderwonk.com/blog/index.php/archive/adventures-with-gamma-correct-rendering/
float3 linear_to_srgb(float3 color)
{
	return pow(abs(color), 1.0f / 2.2f);
}

float3 srgb_to_linear(float3 color)
{
	return pow(abs(color), 2.2f);
}

// http://frictionalgames.blogspot.com/2012/09/tech-feature-hdr-lightning.html
float3 tonemap_uncharted2(float3 x)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;

	return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
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

	const float3 base_color = srgb_to_linear(gbuffer_base_color_texture.Sample(default_sampler, input.texcoord).xyz);
	const float2 metalness_roughness = gbuffer_metalness_roughness_texture.Sample(default_sampler, input.texcoord).bg;
	const float metalness = metalness_roughness.r;
	const float linear_roughness = max(0.1, metalness_roughness.g * metalness_roughness.g);
	const float3 normal_ws = gbuffer_normal_map_texture.Sample(default_sampler, input.texcoord).xyz * 2 - 1;

	const float3 position_ws = position_ws_from_depth(depth, input.texcoord, projection_matrix, inverse_view_projection_matrix);
	const float3 direction_to_camera_ws = normalize(camera_position_ws - position_ws.xyz);

	const float3 diffuse_color = base_color * (1.0f - metalness);
	const float3 specular_color = lerp(0.04f, base_color, metalness);

	const float n_dot_v = saturate(dot(normal_ws, direction_to_camera_ws));

	float3 indirect_lighting = float3(0.1, 0.1, 0.1);

	float3 direct_lighting = float3(0.0, 0.0, 0.0);

	{
		const float3 radiance = float3(1.0, 1.0, 1.0) * 10;

		const float3 direction_to_light_ws = normalize(-sun_light.direction_ws);
		const float3 half_vector_ws = normalize(direction_to_light_ws + direction_to_camera_ws);

		const float n_dot_l = saturate(dot(normal_ws, direction_to_light_ws));
		const float n_dot_h = saturate(dot(normal_ws, half_vector_ws));
		const float v_dot_h = saturate(dot(direction_to_camera_ws, half_vector_ws));
		const float l_dot_h = dot(direction_to_light_ws, half_vector_ws);

		const float3 f_s = specular(specular_color, n_dot_v, n_dot_l, n_dot_h, v_dot_h, l_dot_h, linear_roughness);
		const float3 f_d = diffuse(diffuse_color);

		direct_lighting += (f_s + f_d) * n_dot_l * radiance;
	}

	float3 lighting = direct_lighting + indirect_lighting;

	//lighting = tonemap_uncharted2(lighting);

	lighting = linear_to_srgb(lighting);

	return float4(lighting, 1.0);
}