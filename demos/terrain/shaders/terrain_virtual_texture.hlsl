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

	float layers[] = {
		1.0f,
		snoise(texcoord * 0.1) * 0.5 + 0.5,
		snoise(texcoord * 0.5) * 0.5 + 0.5
	};

	float3 color = float3(0.0f, 0.0f, 0.0f);

	for (int i = 0; i < 3; ++i)
	{
		color = lerp(color, input_textures[i].SampleLevel(linear_clamp, texcoord, 0).rgb, layers[i]);
	}

	output_texture[thread_id.xy] = float4(color, 1.0);
}