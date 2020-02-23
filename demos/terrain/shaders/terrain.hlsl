#include "../../../sparky/shaders/sparky/noise.hlsli"

Texture2D terrain_virtual_texture : register(t0);
SamplerState linear_clamp : register(s0);

cbuffer per_frame_cbuffer : register(b0)
{
	float4x4 view_matrix;
	float4x4 projection_matrix;
	float4x4 view_projection_matrix;
	float4x4 inverse_view_matrix;
	float4x4 inverse_projection_matrix;
	float4x4 inverse_view_projection_matrix;
	float3 camera_position_ws;
	float dummy;
}

cbuffer per_draw_cbuffer : register(b6)
{
	float4x4 world_matrix;
}

struct vs_input
{
	float3 position_os : POSITION;
};

struct vs_output
{
	float4 position_cs : SV_Position;
    float3 normal_ws : NORMAL;
    float2 texcoord : TEXCOORD;
};

vs_output vs_main(vs_input input)
{
    float4x4 world_view_projection_matrix = mul(world_matrix, view_projection_matrix);

    float3 position_os = input.position_os;

    float3 signal = snoise_grad(position_os.xz * 0.1);

    position_os.y = signal.z;

    vs_output output;

    output.position_cs = mul(float4(position_os, 1.0), world_view_projection_matrix);
    output.normal_ws = mul(float4(signal.x, 1.0, signal.y, 0.0f), world_matrix).xyz;
    output.texcoord = position_os.xz * 0.01;

	return output;
}

struct ps_input
{
	float4 position_ss : SV_Position;
    float3 normal_ws : NORMAL;
    float2 texcoord : TEXCOORD;
};

float4 ps_main(ps_input input) : SV_Target0
{
    float4 color = terrain_virtual_texture.SampleLevel(linear_clamp, input.texcoord, 0);

    return color * dot(normalize(input.normal_ws), float3(0.0, 1.0, 0.0));
}