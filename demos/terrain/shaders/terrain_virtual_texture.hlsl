Texture2D input_textures[4] : register(t0);
RWTexture2D<float4> output_texture : register(u0);
SamplerState linear_clamp : register(s0);

[numthreads(8, 8, 1)]
void cs_main(uint3 thread_id : SV_DispatchThreadID)
{
	uint width, height;
	input_textures[0].GetDimensions(width, height);

	float2 texcoord = thread_id.xy / float2(width, height);

	float4 color = 0.33 * input_textures[0].SampleLevel(linear_clamp, texcoord, 0);
	color += 0.33 * input_textures[1].SampleLevel(linear_clamp, texcoord, 0);
	color += 0.33 * input_textures[2].SampleLevel(linear_clamp, texcoord, 0);

	output_texture[thread_id.xy] = color;
}