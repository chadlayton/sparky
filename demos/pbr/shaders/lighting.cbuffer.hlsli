#define CBUFFER_LIGHTING_REGISTER 1

#if defined(__cplusplus)
#define CBUFFER_DECLARE(X,N) __declspec(align(16)) struct X
#else
#define CBUFFER_DECLARE(X,N) cbuffer X : register(b ## N)
#endif

CBUFFER_DECLARE(constant_buffer_lighting_per_frame_data, CBUFFER_LIGHTING_REGISTER)
{
	float image_based_lighting_scale = 1.0f;
	int sampling_method = 0;
};

#undef CBUFFER_DECLARE