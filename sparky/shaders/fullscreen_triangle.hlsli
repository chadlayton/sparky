// http://www.slideshare.net/DevCentralAMD/vertex-shader-tricks-bill-bilodeau

void fullscreen_triangle_cw( in uint vertex_id, out float4 position_cs, out float2 texcoord )
{
	position_cs.x = (float)(vertex_id / 2) * 4.0 - 1.0;
	position_cs.y = (float)(vertex_id % 2) * 4.0 - 1.0;
	position_cs.z = 0.0;
	position_cs.w = 1.0;

	// Generate the texture coordinates
	texcoord.x = (float)(vertex_id / 2) * 2.0f;
	texcoord.y = 1.0f - (float)(vertex_id % 2) * 2.0f;
}

void fullscreen_triangle_ccw( in uint vertex_id, out float4 position_cs, out float2 texcoord )
{
	position_cs.x = (float)((2 - vertex_id) / 2) * 4.0 - 1.0;
	position_cs.y = (float)((2 - vertex_id) % 2) * 4.0 - 1.0;
	position_cs.z = 0.0;
	position_cs.w = 1.0;

	// Generate the texture coordinates
	texcoord.x = (float)((2 - vertex_id) / 2) * 2.0f;
	texcoord.y = 1.0f - (float)((2 - vertex_id) % 2) * 2.0f;
}

struct vs_input
{
	uint vertex_id : SV_VertexID;
};

struct vs_output
{
	float4 position_cs : SV_Position;
	float2 texcoord : TEXCOORD;
};

vs_output vs_main( vs_input input )
{
	vs_output output;

	fullscreen_triangle_ccw( input.vertex_id, output.position_cs, output.texcoord );

	return output;
}

struct ps_input
{
	float4 position_ss : SV_Position;
	float2 texcoord : TEXCOORD;
};