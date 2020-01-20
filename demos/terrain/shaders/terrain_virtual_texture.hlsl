#include "../../pbr/shaders/noise.hlsli"

Texture2D input_textures[3] : register(t0);
RWTexture2D<float4> output_texture : register(u0);
SamplerState linear_clamp : register(s0);

[numthreads(8, 8, 1)]
void cs_main(uint3 thread_id : SV_DispatchThreadID)
{
	uint width, height;
	input_textures[0].GetDimensions(width, height);

	float2 texcoord = thread_id.xy / float2(width, height);

	float3 color = input_textures[0].SampleLevel(linear_clamp, texcoord, 0).rgb;

	for (int i = 1; i < 3; ++i)
	{
		color = lerp(color, input_textures[i].SampleLevel(linear_clamp, texcoord, 0).rgb, snoise(texcoord));
	}

	output_texture[thread_id.xy] = float4(color, 1.0);
}