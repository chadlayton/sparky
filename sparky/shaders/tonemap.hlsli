#define TONEMAP_NONE 0
#define TONEMAP_UNCHARTED2 1

#if !defined(TONEMAP_FUNC)
#define TONEMAP_FUNC TONEMAP_UNCHARTED2
#endif

float3 tonemap(float3 x)
{
#if TONEMAP_FUNC == TONEMAP_NONE
	return x;
#elif TONEMAP_FUNC == TONEMAP_UNCHARTED2
	// http://frictionalgames.blogspot.com/2012/09/tech-feature-hdr-lightning.html
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;

	return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
#else
#error
#endif
}