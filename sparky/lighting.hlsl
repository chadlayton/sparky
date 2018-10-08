Texture2D gbuffer_base_color_texture : register(t0);
Texture2D gbuffer_normal_map_texture : register(t1);
Texture2D gbuffer_depth_texture : register(t2);

SamplerState default_sampler : register(s0);

cbuffer per_frame_cbuffer : register(b0) // per_batch, per_instance, per_material, etc
{
	float4x4 projection_matrix;
	float4x4 view_projection_matrix;
	float4x4 inverse_view_projection_matrix;
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

// http://www.slideshare.net/DevCentralAMD/vertex-shader-tricks-bill-bilodeau
void fullscreen_triangle(in uint vertex_id, out float4 position_cs, out float2 texcoord)
{
	// Generate the clip-space position
	position_cs.x = (float)(vertex_id / 2) * 4.0 - 1.0;
	position_cs.y = (float)(vertex_id % 2) * 4.0 - 1.0;
	position_cs.z = 0.0;
	position_cs.w = 1.0;

	// Generate the texture coordinates
	texcoord.x = (float)(vertex_id / 2) * 2.0f;
	texcoord.y = 1.0f - (float)(vertex_id % 2) * 2.0f;
}

float3 position_ws_from_depth(in float depth_post_projection, in float2 texcoord)
{
	float linear_depth = projection_matrix._43 / (depth_post_projection - projection_matrix._33);
	float4 position_cs = float4(texcoord * 2.0f - 1.0f, linear_depth, 1.0f);
	position_cs.y *= -1.0f;
	float4 position_ws = mul(inverse_view_projection_matrix, position_cs);
	return position_ws.xyz / position_ws.w;
}

vs_output vs_main(vs_input input)
{
	vs_output output;

	fullscreen_triangle(input.vertex_id, output.position_cs, output.texcoord);

	return output;
}

struct point_light
{
	float3 position_ws;
	float intensity;      // radiant intensity (power per unit steradian)
};

struct ps_input
{
	float4 position_ss : SV_Position;
	float2 texcoord : TEXCOORD;
};

float4 ps_main(ps_input input) : SV_Target0
{
	point_light light;
	light.position_ws = float3(0.0f, 0.0f, -10.0f);
	light.intensity = 10;

	float depth = gbuffer_depth_texture.Sample(default_sampler, input.texcoord).r;

	float3 base_color = gbuffer_base_color_texture.Sample(default_sampler, input.texcoord).xyz;
	float3 normal_ws = gbuffer_normal_map_texture.Sample(default_sampler, input.texcoord).xyz * 2 - 1;
	float3 position_ws = position_ws_from_depth(depth, input.texcoord);

	float3 direction_to_light = normalize(light.position_ws - position_ws);
	float n_dot_l = saturate(dot(normal_ws, direction_to_light));
	float3 color = base_color * n_dot_l;

	return float4(color, 1.0f);
}