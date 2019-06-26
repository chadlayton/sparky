// https://kosmonautblog.wordpress.com/2017/03/26/designing-a-linear-hdr-pipeline/
// http://renderwonk.com/blog/index.php/archive/adventures-with-gamma-correct-rendering/
float3 linear_to_srgb(float3 color)
{
	return pow(abs(color), 1.0f / 2.2f);
}

float4 linear_to_srgb(float4 color)
{
	return float4(linear_to_srgb(color.rgb), color.a);
}

float3 srgb_to_linear(float3 color)
{
	return pow(abs(color), 2.2f);
}

float4 srgb_to_linear(float4 color)
{
	return float4(srgb_to_linear(color.rgb), color.a);
}