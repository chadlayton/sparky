Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

struct vs_input
{
	float4 position_os : POSITION;
	float4 normal_os : NORMAL;
	float2 texcoord : TEXCOORD;
	float4 color : COLOR;
};

struct vs_output
{
	float4 position_cs : SV_Position;
	float4 position_ws : POSITION;
	float4 normal_ws : NORMAL;
	float2 texcoord : TEXCOORD;
	float4 color : COLOR;
};

vs_output VSMain(vs_input input)
{
	vs_output output;

	output.position_cs = input.position_os;
	output.position_ws = input.position_os;
	output.normal_ws = input.normal_os;
	output.texcoord = input.texcoord;
	output.color = input.color;

	return output;
}

struct ps_input
{
	float4 position_ss : SV_Position;
	float4 position_ws : POSITION;
	float4 normal_ws : NORMAL;
	float2 texcoord : TEXCOORD;
	float4 color : COLOR;
};

struct ps_output
{
	float4 base_color : SV_Target0;
	float4 position_ws : SV_Target1;
	float4 normal_ws : SV_Target2;
};

ps_output PSMain(ps_input input)
{
	ps_output output;

	output.base_color = g_texture.Sample(g_sampler, input.texcoord) * input.color;
	output.position_ws = input.position_ws;
	output.normal_ws = input.normal_ws;
	
	return output;
}