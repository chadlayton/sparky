Texture2D base_color_texture : register(t2);
Texture2D normal_map_texture : register(t3);
Texture2D position_texture : register(t4);

SamplerState default_sampler : register(s0);

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

vs_output vs_main(vs_input input)
{
	vs_output output;

	fullscreen_triangle(input.vertex_id, output.position_cs, output.texcoord);

	return output;
}

struct ps_input
{
	float4 position_ss : SV_Position;
	float2 texcoord : TEXCOORD;
};

struct ps_output
{
	float4 base_color : SV_Target0;
	float4 position_ws : SV_Target1;
	float4 normal_ws : SV_Target2;
};

float4 ps_main(ps_input input) : SV_Target0
{
	float3 base_color = base_color_texture.Sample(default_sampler, input.texcoord).xyz;
	float3 normal_ws = normal_map_texture.Sample(default_sampler, input.texcoord).xyz;
	float3 position_ws = position_texture.Sample(default_sampler, input.texcoord).xyz;

	return float4(base_color, 1.0f);
}