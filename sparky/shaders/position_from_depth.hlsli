// https://mynameismjp.wordpress.com/2010/09/05/position-from-depth-3/
float3 position_ws_from_depth(in float depth_post_projection, in float2 texcoord, in float4x4 projection_matrix, in float4x4 inverse_view_projection_matrix)
{
	float linear_depth = projection_matrix._43 / (depth_post_projection - projection_matrix._33);
	float4 position_cs = float4(texcoord * 2.0f - 1.0f, linear_depth, 1.0f);
	position_cs.y *= -1.0f;
	float4 position_ws = mul(position_cs, inverse_view_projection_matrix);
	return position_ws.xyz / position_ws.w;
}