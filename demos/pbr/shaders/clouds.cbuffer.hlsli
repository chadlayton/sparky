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
	float shape_base_erosion_bias = 0.0f;
	float shape_detail_bias = 0.0f;
	float shape_detail_erosion_bias = 0.0f;
	float absorption_factor = 0.01f;
	float scattering_factor = 0.01f;

	float cloud_layer_height_begin = 1500.0f;
	float cloud_layer_height_end = 4000.0f;

	float ambient_light = 1.0f;
	float sun_light = 100.0f;

	float step_size_ws = 100.0f;
	float shadow_step_size_ws = 100.0f;

	float debug0 = 0.0f;
	float debug1 = 0.0f;
	float debug2 = 0.0f;
	float debug3 = 0.0f;

	int debug_raymarch_termination = 0;
	int debug_shadow_raymarch_termination = 0;
	int debug_shadow_raymarch_termination_step = 0;
	int debug_toggle_frostbite_scattering = 0;
};

#undef CBUFFER_DECLARE