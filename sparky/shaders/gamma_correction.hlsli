// https://kosmonautblog.wordpress.com/2017/03/26/designing-a-linear-hdr-pipeline/
// http://renderwonk.com/blog/index.php/archive/adventures-with-gamma-correct-rendering/
float3 linear_to_srgb(float3 color)
{
	return pow(abs(color), 1.0f / 2.2f);
}

float3 srgb_to_linear(float3 color)
{
	return pow(abs(color), 2.2f);
}