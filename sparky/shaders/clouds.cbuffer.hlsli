#if defined(__cplusplus)
#define VAR(X) __declspec(align(16)) X
#else
#define VAR(X) X
#endif

struct constant_buffer_clouds_per_frame_data
{
	VAR(float weather_sample_scale_bias);
	VAR(float shape_sample_scale_bias);
	VAR(float detail_sample_scale_bias);
	VAR(float density_bias);
	VAR(float coverage_bias);
	VAR(float type_bias);
	VAR(float shape_base_bias);
	VAR(float shape_detail_bias);
	VAR(float extinction_coeff_bias);
	VAR(float scattering_coeff_bias);
	VAR(float debug0);
	VAR(float debug1);
	VAR(float debug2);
	VAR(float debug3);
	VAR(float debug4);
};