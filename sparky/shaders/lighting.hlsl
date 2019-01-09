Texture2D gbuffer_base_color_texture : register(t0);
Texture2D gbuffer_metalness_roughness_texture : register(t1);
Texture2D gbuffer_normal_map_texture : register(t2);
Texture2D gbuffer_depth_texture : register(t3);

SamplerState default_sampler : register(s0);

static const float PI = 3.14159265f;
static const float EPSILON = 1e-10;

cbuffer per_frame_cbuffer : register(b0) // per_batch, per_instance, per_material, etc
{
	float4x4 projection_matrix;
	float4x4 view_projection_matrix;
	float4x4 inverse_view_projection_matrix;
	float3 camera_position_ws;
	float3 sun_direction_ws;
};

struct vs_input
{
	uint vertex_id : SV_VertexID;
};

struct vs_output
{
	float4 position_cs : SV_Position;
	float2 texcoord : TEXCOORD;
};

// http://www.slideshare.net/DevCentralAMD/vertex-shader-tricks-bill-bilodeau
void fullscreen_triangle_cw(in uint vertex_id, out float4 position_cs, out float2 texcoord)
{
	position_cs.x = (float)(vertex_id / 2) * 4.0 - 1.0;
	position_cs.y = (float)(vertex_id % 2) * 4.0 - 1.0;
	position_cs.z = 0.0;
	position_cs.w = 1.0;

	// Generate the texture coordinates
	texcoord.x = (float)(vertex_id / 2) * 2.0f;
	texcoord.y = 1.0f - (float)(vertex_id % 2) * 2.0f;
}

void fullscreen_triangle_ccw(in uint vertex_id, out float4 position_cs, out float2 texcoord)
{
	position_cs.x = (float)((2 - vertex_id) / 2) * 4.0 - 1.0;
	position_cs.y = (float)((2 - vertex_id) % 2) * 4.0 - 1.0;
	position_cs.z = 0.0;
	position_cs.w = 1.0;

	// Generate the texture coordinates
	texcoord.x = (float)((2 - vertex_id) / 2) * 2.0f;
	texcoord.y = 1.0f - (float)((2 - vertex_id) % 2) * 2.0f;
}

float3 position_ws_from_depth(in float depth_post_projection, in float2 texcoord)
{
	float linear_depth = projection_matrix._43 / (depth_post_projection - projection_matrix._33);
	float4 position_cs = float4(texcoord * 2.0f - 1.0f, linear_depth, 1.0f);
	position_cs.y *= -1.0f;
	float4 position_ws = mul(inverse_view_projection_matrix, position_cs);
	return position_ws.xyz / position_ws.w;
}

vs_output vs_main(vs_input input)
{
	vs_output output;

	fullscreen_triangle_ccw(input.vertex_id, output.position_cs, output.texcoord);

	return output;
}

struct point_light
{
	float3 position_ws;
	float3 intensity;      // Radiant intensity. Power per unit steradian.
};

struct directional_light
{
	float3 direction_ws; 
	float3 irradiance;	// Power per unit area received by surface with normal facing direction
};

/**
* The Fresnel reflectance function. Computes the fraction of incoming light that
* is reflected (as opposed to refracted) from an optically flat surface. It varies
* based on the light direction and the surface (or microfacet) normal.
*
* @param l_dot_h The cosine of the angle of incidence. The angle between the light
*                vector L and the surface normal (or the half-vector H in the case
*                of a microfacet BRDF).
* @param f0      The fresnel reflactance at incidence of 0 degrees. The characteristic
*                specular color of the surface.
*
* @returns       The fraction of light reflected.
*/
float3 F_shlick(float l_dot_h, float3 f0)
{
	return f0 + (1.0f - f0) * pow(1.0f - l_dot_h, 5);
}

float3 F_none(float3 f0)
{
	return f0;
}

/**
* The microfacet normal distribution function. Gives the concentration of microfacet
* normals in the direction of the half-angle vector H relative to surface area.
* Controls the size and the shape of the specular highlight.
*
* @param n_dot_h   The cosine of the angle of between the surface normal N and the
*                  half-vector H.
* @param roughness Rougher surfaces have fewer microfacets aligned with the
*                  surface normal N.
*
* @return          The fraction of light reflected.
*/
float D_ggx(float n_dot_h, float roughness)
{
	// Towbridge-Reitz (GGX)
	float roughness2 = roughness * roughness;
	float temp = n_dot_h * n_dot_h * (roughness2 - 1.0f) + 1.0f;
	return roughness2 / (PI * temp * temp);
}

float G_implicit(float n_dot_l, float n_dot_v)
{
	return n_dot_l * n_dot_v;
}

/**
* Specular BRDF. Calculates the ratio of light incoming (radiance) from
* direction L that is reflected from the surface in the direction V.
*
* @return The fraction of light reflected (in inverse steradians).
*/
float3 brdf_specular(float n_dot_v, float n_dot_l, float n_dot_h, float l_dot_h, float3 f0, float roughness)
{
	// Cook-Torrance
	float3 F = F_shlick(l_dot_h, f0);
	float D = D_ggx(n_dot_h, roughness);
	float G = G_implicit(n_dot_l, n_dot_v);

	// Visibility term
	float V = G / (n_dot_l * n_dot_v + EPSILON);

	return (1.0f / 4.0f) * F * D * V;
}

float3 brdf_diffuse(float3 base_color)
{
	// Lambert
	return base_color / PI;
}

// http://frictionalgames.blogspot.com/2012/09/tech-feature-hdr-lightning.html
float3 tonemap_uncharted2(float3 x)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;

	return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

struct ps_input
{
	float4 position_ss : SV_Position;
	float2 texcoord : TEXCOORD;
};

float4 ps_main(ps_input input) : SV_Target0
{
	directional_light sun_light;
	sun_light.direction_ws = sun_direction_ws;
	sun_light.irradiance = 10;

	float depth = gbuffer_depth_texture.Sample(default_sampler, input.texcoord).r;

	// TODO: Need some kind of mask for sky
	//if (depth > 0.99)
	//{
	//	return float4(0.0f, 0.3f, 0.8f, 1.0f);
	//}

	// Surface properties
	float3 base_color = gbuffer_base_color_texture.Sample(default_sampler, input.texcoord).xyz;
	float2 metalness_roughness = gbuffer_metalness_roughness_texture.Sample(default_sampler, input.texcoord).bg;
	float metalness = metalness_roughness.r;
	float disney_roughness = metalness_roughness.g * metalness_roughness.g;
	float3 normal_ws = gbuffer_normal_map_texture.Sample(default_sampler, input.texcoord).xyz * 2 - 1;
	float3 position_ws = position_ws_from_depth(depth, input.texcoord);
	float3 direction_to_camera_ws = normalize(camera_position_ws - position_ws);

	float3 diffuse_color = base_color * (1.0f - metalness);
	float3 specular_color = lerp(0.04f, base_color, metalness);

	float3 N = normal_ws;
	float3 V = direction_to_camera_ws;

	float n_dot_v = saturate(dot(N, V));

	float3 indirect_lighting = float3(0.1, 0.1, 0.2);

	float3 direct_lighting = float3(0.0, 0.0, 0.0);

	{
		float3 light_color = float3(3.0, 3.0, 3.0);

		float3 direction_to_light_ws = normalize(-sun_light.direction_ws);

		float3 L = direction_to_light_ws;
		float3 H = normalize(L + V);

		float n_dot_l = saturate(dot(N, L));
		float n_dot_h = saturate(dot(N, H));
		float v_dot_h = saturate(dot(V, H));
		float l_dot_h = dot(L, H);

		direct_lighting += (brdf_diffuse(diffuse_color) + brdf_specular(n_dot_v, n_dot_l, n_dot_h, l_dot_h, specular_color, disney_roughness)) * PI * n_dot_l * light_color;
	}

	float3 lighting = direct_lighting + indirect_lighting;

	// lighting = tonemap_uncharted2(lighting);

	return float4(base_color, lighting.r + lighting.g + lighting.b);
}