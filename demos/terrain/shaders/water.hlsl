#include "../../pbr/shaders/noise.hlsli"

#define PI 3.1415927

Texture2D test_texture : register(t0);
SamplerState linear_wrap : register(s0);

cbuffer per_frame_cbuffer : register(b0)
{
	float4x4 view_matrix;
	float4x4 projection_matrix;
	float4x4 view_projection_matrix;
	float4x4 inverse_view_matrix;
	float4x4 inverse_projection_matrix;
	float4x4 inverse_view_projection_matrix;
	float3 camera_position_ws;
	float time_ms;
}

cbuffer per_draw_cbuffer : register(b6)
{
	float4x4 world_matrix;
}

struct vs_input
{
	float3 position_os : POSITION;
};

struct vs_output
{
	float4 position_cs : SV_Position;
	float3 position_ws : POSITION_WS;
	float3 normal_ws : NORMAL_WS;
};

float remap(float value, float inMin, float inMax, float outMin, float outMax) 
{
	return outMin + (outMax - outMin) * (value - inMin) / (inMax - inMin);
}

#define FLOW_DEBUG_FLOW_DIR_OVERRIDE_ENABLED 0
#define FLOW_DEBUG_FLOW_DIR_OVERRIDE float2(1.0, 0.0)

#define FLOW_DEBUG_SHOW_FLOW_DIR_ENABLED 0
#define FLOW_DEBUG_SHOW_PHASE_ENABLED 0

#define FLOW_PHASE_OFFSET_ENABLED 1
#define FLOW_LAYER_TWO_ANIMATION_OFFSET_ENABLED 1
#define FLOW_TIME_SCALE 0.0005
#define FLOW_INTENSITY 1.0
#define FLOW_PHASE_OFFSET_FUNC(X) snoise(X) * 0.5 + 0.5
#define FLOW_LAYER_FUNC(X) test_texture.SampleLevel(linear_wrap, X, 0)

float4 Flow(float2 uv, float2 dir, float t)
{
	dir *= FLOW_INTENSITY;

#if FLOW_DEBUG_FLOW_DIR_OVERRIDE_ENABLED
	dir = FLOW_DEBUG_FLOW_DIR_OVERRIDE;
#endif

#if FLOW_DEBUG_SHOW_FLOW_DIR_ENABLED
	return float3(dir * 0.5 + 0.5, 0.0);
#endif

	t *= FLOW_TIME_SCALE;

#if FLOW_PHASE_OFFSET_ENABLED
	t += FLOW_PHASE_OFFSET_FUNC(uv);
#endif

	float p0 = frac(t);
	float p1 = frac(t + 0.5);

#if FLOW_DEBUG_SHOW_PHASE_ENABLED
	return float3(p0, p1, 0.0);
#endif

	float b0 = abs(1.0 - 2.0 * p0);
	float b1 = abs(1.0 - 2.0 * p1);

	float2 uv0 = uv - (dir * p0);
	float2 uv1 = uv - (dir * p1);

#if FLOW_LAYER_TWO_ANIMATION_OFFSET_ENABLED
	uv1 += float2(0.5, 0.5);
#endif

	float4 l0 = FLOW_LAYER_FUNC(uv0);
	float4 l1 = FLOW_LAYER_FUNC(uv1);

	return (l0 * b0) + (l1 * b1);
}

vs_output vs_main(vs_input input)
{
    float4x4 world_view_projection_matrix = mul(world_matrix, view_projection_matrix);

    vs_output output;

	float3 position_os = input.position_os;

	float t = fmod(position_os.x + time_ms * 0.001, PI * 10.0);
	t = remap(t, 0, 10, -PI, PI);

	position_os.x += t;
	position_os.y = (cos(t) - 1) * 0.5;

	float3 tangent_os = float3(0.0, -sin(t), 0.0);
	float3 normal_os = tangent_os.yxz;

    output.position_cs = mul(float4(position_os, 1.0), world_view_projection_matrix);
	output.position_ws = mul(float4(position_os, 1.0), world_matrix).xyz;
	output.normal_ws = mul(float4(normal_os, 0.0), world_matrix).xyz;

	return output;
}

struct ps_input
{
	float4 position_ss : SV_Position;
	float3 position_ws : POSITION_WS;
	float3 normal_ws : NORMAL_WS;
};

float4 ps_main(ps_input input) : SV_Target0
{
	float3 direction_to_sun_ws = float3(1.0, 1.0, 0.0);
	direction_to_sun_ws = normalize(direction_to_sun_ws);

	float flow_dir_uv_scale = 0.02;
	float2 flow_dir = float2(snoise(input.position_ws.xz * flow_dir_uv_scale), snoise(input.position_ws.zx * flow_dir_uv_scale));
	flow_dir = normalize(flow_dir);

	float4 flow = Flow(input.position_ws.xz * 0.01, flow_dir, time_ms);

	float3 normal_ws = input.normal_ws;

	normal_ws.x += flow.x;
	normal_ws.z += flow.y;

	normal_ws = normalize(normal_ws);

	float3 n_dot_l = saturate(dot(normal_ws, direction_to_sun_ws));

	return float4(flow * n_dot_l, 1.0);
}