Texture2D<float4> input : register(t0);
RWTexture2D<float4> output : register(u0);
SamplerState linear_clamp : register(s0);

[numthreads(8, 8, 1)]
void cs_main(uint3 thread_id : SV_DispatchThreadID)
{
	uint width, height;
	input.GetDimensions(width, height);

	float2 texel_size = 1.0 / float2(width / 2, height / 2);

	float2 texcoord = (thread_id.xy + 0.5) * texel_size;

	output[thread_id.xy] = input.SampleLevel(linear_clamp, texcoord, 0);
}