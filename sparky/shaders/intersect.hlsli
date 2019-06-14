bool intersect_ray_sphere(float3 sphere_center, float sphere_radius, float3 ray_origin, float3 ray_dir, out float t)
{
	const float3 L = sphere_center - ray_origin;
	const float t_ca = dot(L, ray_dir);
	const float d2 = dot(L, L) - t_ca * t_ca;
	const float r2 = sphere_radius * sphere_radius;

	if (d2 > r2)
	{
		t = 0.0;
		return false;
	}

	const float t_hc = sqrt(r2 - d2);

	float t_0 = t_ca - t_hc;
	float t_1 = t_ca + t_hc;

	if (t_0 > t_1)
	{
		const float temp = t_1;
		t_0 = t_1;
		t_1 = temp;
	}

	if (t_0 < 0)
	{
		t_0 = t_1;

		if (t_0 < 0)
		{
			t = 0.0;
			return false;
		}
	}

	t = t_0;

	return true;
}

bool intersect_ray_plane(float3 plane_normal, float3 point_on_plane, float3 ray_origin, float3 ray_dir, out float t)
{
	float d = dot(ray_dir, plane_normal);

	if (d <= 0)
	{
		t = 0.0;
		return false;
	}

	t = dot(point_on_plane - ray_origin, plane_normal) / d;

	return true;
}
