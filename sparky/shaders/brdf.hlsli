#define BRDF_SPECULAR_D_GGX      0

#define BRDF_SPECULAR_F_NONE     0
#define BRDF_SPECULAR_F_SCHLICK  1

#define BRDF_SPECULAR_V_IMPLICIT 0
#define BRDF_SPECULAR_V_GGX      1

#define BRDF_SPECULAR_D BRDF_SPECULAR_D_GGX
#define BRDF_SPECULAR_F BRDF_SPECULAR_F_SCHLICK 
#define BRDF_SPECULAR_V BRDF_SPECULAR_V_GGX

/**
* The Fresnel reflectance function F. Computes the fraction of incoming light that
* is reflected (as opposed to refracted) from an optically flat surface. It varies
* based on the light direction and the surface (or microfacet) normal.
*
* @param l_dot_h The cosine of the angle of incidence. The angle between the light
*                vector L and the surface normal. Or, in the case of a microfacet 
*                BRDF, the microfacet normal equivalent to the half-vector H.
* @param f0      The fresnel reflectance at incidence of 0 degrees. The 
*                characteristic specular color of the surface.
*
* @returns       The fraction of light reflected.
*/
float3 fresnel(float3 f0, float l_dot_h)
{
#if BRDF_SPECULAR_F == BRDF_SPECULAR_F_NONE
	return f0;
#elif BRDF_SPECULAR_F == BRDF_SPECULAR_F_SCHLICK
	return f0 + (1.0f - f0) * pow(1.0f - l_dot_h, 5);
#endif
}

/**
* The microfacet normal distribution function G gives the concentration of 
* microfacet normals in the direction of the half-vector H relative to surface 
* area. Controls the size and the shape of the specular highlight. Because many 
* BRDFs replace both G in the numerator and the (n_dot_l * n_dot_v) 
* “foreshortening factor” in the denominator, it's useful to combine them into a
* single "visibility" term V.
*
* @param n_dot_v
* @param n_dot_l
* @param alpha   Linear surface roughness. Rougher surfaces have fewer microfacets 
*                aligned with the surface normal N.
*
* @return        The fraction of light reflected divided by the BRDF "foreshortening factor".
*/
float visibility(float n_dot_v, float n_dot_l, float alpha)
{
#if BRDF_SPECULAR_V == BRDF_SPECULAR_V_IMPLICIT
	// Implicit
	// G_implicit = n_dot_l * n_dot_v
	// V = G_implicit / (n_dot_l * n_dot_v) 
	//   = 1

	return 1.0f;
#elif BRDF_SPECULAR_V == BRDF_SPECULAR_V_GGX
	// Smith Correlated (GGX)

	const float alpha2 = alpha * alpha;
	const float lambda_v = sqrt(alpha2 + (1.0f - alpha2) * (n_dot_v * n_dot_v));
	const float lambda_l = sqrt(alpha2 + (1.0f - alpha2) * (n_dot_l * n_dot_l));

	return 2.0f / (n_dot_v * lambda_l + n_dot_l * lambda_v);
#endif
}


/**
* The microfacet normal distribution function. Gives the concentration of microfacet
* normals in the direction of the half-vector H relative to surface area. Controls 
* the size and the shape of the specular highlight.
*
* @param n_dot_h The cosine of the angle of between the surface normal N and the
*                half-vector H.
* @param alpha   Linear surface roughness. Rougher surfaces have fewer microfacets
*                aligned with the surface normal N.
*
* @return        The fraction of microfacets oriented such that they could reflect
*                light from L towards V.
*/
float distribution(float n_dot_h, float alpha)
{
#if BRDF_SPECULAR_D == BRDF_SPECULAR_D_GGX
	// Towbridge-Reitz (GGX)

	float alpha2 = alpha * alpha;
	float temp = n_dot_h * n_dot_h * (alpha2 - 1.0f) + 1.0f;
	return alpha2 / (PI * temp * temp);
#endif
}

/**
* Specular BRDF. Calculates the ratio of light incoming (radiance) from
* direction L that is reflected from the surface in the direction V.
*
* @return The fraction of light reflected (in inverse steradians sr^-1) 
*         in the direction of V.
*/
float3 specular(float3 specular_color, float n_dot_v, float n_dot_l, float n_dot_h, float v_dot_h, float l_dot_h, float alpha)
{
	// Cook-Torrance BRDF
	// V = G / (n_dot_l * n_dot_v)
	// f_s = (F * G * D) / (4 * n_dot_l * n_dot_v) 
	//     = (F * D * V) / 4

	const float3 F = fresnel(specular_color, l_dot_h);
	const float V = visibility(n_dot_v, n_dot_l, alpha);
	const float D = distribution(n_dot_h, alpha);

	return (F * D * V) / 4.0f;
}

/**
* Diffuse BRDF. Calculates the ratio of light reflected in all directions.
*
* @param diffuse_color Corresponds with what most consider the "surface color"
*
* @return              The fraction of light reflected (in inverse steradians sr^-1)
*                      independent of V.
*/
float3 diffuse(float3 diffuse_color)
{
	// Lambert BRDF. The well-known n_dot_l term is part of the reflectance 
	// equation, not the BRDF.
	return diffuse_color / PI;
}

// Reference
// http://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html
// https://google.github.io/filament/Materials.md.html
// https://blog.selfshadow.com/publications/s2015-shading-course/hoffman/s2015_pbs_physics_math_slides.pdf