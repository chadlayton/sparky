Texture2D base_color_texture : register(t0);
//Texture2D normal_texture : register(t1);

SamplerState default_sampler : register(s0);

cbuffer per_frame_cbuffer : register(b0) // per_batch, per_instance, per_material, etc
{
	float4x4 projection_matrix;
	float4x4 view_projection_matrix;
	float4x4 inverse_view_projection_matrix;
};

struct vs_input
{
	float3 position_os : POSITION;
	float3 normal_os : NORMAL;
	float2 texcoord : TEXCOORD;
	float4 color : COLOR;
};

struct vs_output
{
	float4 position_cs : SV_Position;
	float4 normal_ws : NORMAL;
	float2 texcoord : TEXCOORD;
	float4 color : COLOR;
};

vs_output vs_main(vs_input input)
{
	float4x4 world_view_projection_matrix = view_projection_matrix;

	vs_output output;

	output.position_cs = mul(float4(input.position_os, 1.0f), world_view_projection_matrix);
	output.normal_ws = mul(float4(input.normal_os, 0.0f), world_view_projection_matrix);
	output.texcoord = input.texcoord;
	output.color = input.color;

	return output;
}

struct ps_input
{
	float4 position_ss : SV_Position;
	float4 normal_ws : NORMAL;
	float2 texcoord : TEXCOORD;
	float4 color : COLOR;
};

struct ps_output
{
	float4 base_color : SV_Target0;
	float4 normal_ws : SV_Target1;
};

ps_output ps_main(ps_input input)
{
	ps_output output;

	output.normal_ws = (input.normal_ws + 1) / 2;
	output.base_color = base_color_texture.Sample(default_sampler, input.texcoord) * input.color;

	return output;
}