cbuffer per_frame_cbuffer : register(b0) // per_batch, per_instance, per_material, etc
{
	float4x4 view_matrix;
	float4x4 projection_matrix;
	float4x4 view_projection_matrix;
	float4x4 inverse_view_matrix;
	float4x4 inverse_projection_matrix;
	float4x4 inverse_view_projection_matrix;
	float3 camera_position_ws;
	float3 sun_direction_ws;
};