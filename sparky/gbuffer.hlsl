#if BINDLESS_TEXTURES
Texture2D textures_2d[12 /* Must match numDescriptors when creating root signature */] : register(t0, space0);
#else
Texture2D base_color_texture : register(t0);
Texture2D metalness_roughness_texture : register(t1);
#endif

SamplerState default_sampler : register(s0);

cbuffer per_frame_cbuffer : register(b0) // per_batch, per_instance, per_material, etc
{
	float4x4 projection_matrix;
	float4x4 view_projection_matrix;
	float4x4 inverse_view_projection_matrix;
};

cbuffer per_object_cbuffer : register(b1)
{
	float4x4 world_matrix;
}

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
	float3 normal_ws : NORMAL;
	float2 texcoord : TEXCOORD;
	float4 color : COLOR;
};

vs_output vs_main(vs_input input)
{
	float4x4 world_view_projection_matrix = mul(world_matrix, view_projection_matrix);

	vs_output output;

	output.position_cs = mul(float4(input.position_os, 1.0f), world_view_projection_matrix);
	output.normal_ws = mul(float4(input.normal_os, 0.0f), world_matrix).xyz;
	output.texcoord = input.texcoord;
	output.color = input.color;

	return output;
}

struct ps_input
{
	float4 position_ss : SV_Position;
	float3 normal_ws : NORMAL;
	float2 texcoord : TEXCOORD;
	float4 color : COLOR;
};

struct ps_output
{
	float4 base_color : SV_Target0;
	float4 metalness_roughness : SV_Target1;
	float4 normal_ws : SV_Target2;
};

ps_output ps_main(ps_input input)
{
	ps_output output;

#if BINDLESS_TEXTURES
	// TODO: If we were really using bindless then we'd want to pass the texture index
	// in a constant buffer instead (since all textures would share the same array). 
	// e.g. textures_2d[per_object_cbuffer.base_color_texture_index].Sample(...);
	output.base_color = textures_2d[0].Sample(default_sampler, input.texcoord) * input.color;
	output.metalness_roughness = textures_2d[1].Sample(default_sampler, input.texcoord);
#else
	output.base_color = base_color_texture.Sample(default_sampler, input.texcoord) * input.color;
	output.metalness_roughness = metalness_roughness_texture.Sample(default_sampler, input.texcoord);
#endif

	output.normal_ws.xyz = (input.normal_ws + 1) / 2;

	return output;
}