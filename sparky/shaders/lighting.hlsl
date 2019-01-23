Texture2D gbuffer_base_color_texture : register(t0);
Texture2D gbuffer_metalness_roughness_texture : register(t1);
Texture2D gbuffer_normal_map_texture : register(t2);
Texture2D gbuffer_depth_texture : register(t3);

SamplerState default_sampler : register(s0);

static const float PI = 3.14159265f;
static const float EPSILON = 1e-10;

#include "brdf.hlsli"

cbuffer per_frame_cbuffer : register(b0) // per_batch, per_instance, per_material, etc
{
	float4x4 view_matrix;
	float4x4 projection_matrix;
	float4x4 view_projection_matrix;
	float4x4 inverse_view_matrix;
	float4x4 inverse_projection_matrix;
	float4x4 inverse_view_projection_matrix;
	float3 camera_position_ws;
	float3 sun_direction_ws;
};

struct vs_input
{
	uint vertex_id : SV_VertexID;
};

struct vs_output
{
	float4 position_cs : SV_Position;
	float2 texcoord : TEXCOORD;
};

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

// http://www.slideshare.net/DevCentralAMD/vertex-shader-tricks-bill-bilodeau
void fullscreen_triangle_cw(in uint vertex_id, out float4 position_cs, out float2 texcoord)
{
	position_cs.x = (float)(vertex_id / 2) * 4.0 - 1.0;
	position_cs.y = (float)(vertex_id % 2) * 4.0 - 1.0;
	position_cs.z = 0.0;
	position_cs.w = 1.0;

	// Generate the texture coordinates
	texcoord.x = (float)(vertex_id / 2) * 2.0f;
	texcoord.y = 1.0f - (float)(vertex_id % 2) * 2.0f;
}

void fullscreen_triangle_ccw(in uint vertex_id, out float4 position_cs, out float2 texcoord)
{
	position_cs.x = (float)((2 - vertex_id) / 2) * 4.0 - 1.0;
	position_cs.y = (float)((2 - vertex_id) % 2) * 4.0 - 1.0;
	position_cs.z = 0.0;
	position_cs.w = 1.0;

	// Generate the texture coordinates
	texcoord.x = (float)((2 - vertex_id) / 2) * 2.0f;
	texcoord.y = 1.0f - (float)((2 - vertex_id) % 2) * 2.0f;
}

// https://mynameismjp.wordpress.com/2010/09/05/position-from-depth-3/
float3 position_ws_from_depth(in float depth_post_projection, in float2 texcoord)
{
	float linear_depth = projection_matrix._43 / (depth_post_projection - projection_matrix._33);
	float4 position_cs = float4(texcoord * 2.0f - 1.0f, linear_depth, 1.0f);
	position_cs.y *= -1.0f;
	float4 position_ws = mul(position_cs, inverse_view_projection_matrix);
	return position_ws.xyz / position_ws.w;
}

vs_output vs_main(vs_input input)
{
	vs_output output;

	fullscreen_triangle_ccw(input.vertex_id, output.position_cs, output.texcoord);

	return output;
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

struct ps_input
{
	float4 position_ss : SV_Position;
	float2 texcoord : TEXCOORD;
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

	const float3 base_color = srgb_to_linear(gbuffer_base_color_texture.Sample(default_sampler, input.texcoord).xyz);
	const float2 metalness_roughness = gbuffer_metalness_roughness_texture.Sample(default_sampler, input.texcoord).bg;
	const float metalness = metalness_roughness.r;
	const float linear_roughness = max(0.1, metalness_roughness.g * metalness_roughness.g);
	const float3 normal_ws = gbuffer_normal_map_texture.Sample(default_sampler, input.texcoord).xyz * 2 - 1;

	const float3 position_ws = position_ws_from_depth(depth, input.texcoord);
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