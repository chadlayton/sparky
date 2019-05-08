#define CBUFFER_CLOUDS_REGISTER 1

#if defined(__cplusplus)
#define CBUFFER_DECLARE(X,N) __declspec(align(16)) struct X
#else
#define CBUFFER_DECLARE(X,N) cbuffer X : register(b ## N)
#endif

CBUFFER_DECLARE(constant_buffer_clouds_per_frame_data, CBUFFER_CLOUDS_REGISTER)
{
	float weather_sample_scale_bias = 0.0f;
	float shape_sample_scale_bias = 0.0f;
	float detail_sample_scale_bias = 0.0f;
	float density_bias = 0.0f;
	float coverage_bias = 0.0f;
	float type_bias = 0.0f;
	float shape_base_bias = 0.0f;
	float shape_detail_bias = 0.0f;
	float extinction_coeff = 1.0f;
	float scattering_coeff = 1.0f;

	float cloud_layer_height_begin = 1500.0f;
	float cloud_layer_height_end = 4000.0f;

	float debug0 = 0.0f;
	float debug1 = 0.0f;
	float debug2 = 0.0f;
	float debug3 = 0.0f;
};

#undef CBUFFER_DECLARE