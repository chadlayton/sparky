RWTexture3D<float4> output : register(u0);

[numthreads(8, 8, 8)]
void cs_main(uint3 thread_id : SV_DispatchThreadID)
{
	uint width, height, depth;

	output.GetDimensions(width, height, depth);
	float3 texcoord = thread_id.xyz / float3(width, height, depth);

	output[thread_id] = float4(texcoord, 1.0f);
}