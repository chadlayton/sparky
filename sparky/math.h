#pragma once

#include <cassert>
#include <cmath>
#include <cstdint>
#include <array>
#include <initializer_list>

namespace math
{
	constexpr float pi = 3.1415927f;
	constexpr float pi_div_2 = pi / 2;
	constexpr float pi_div_4 = pi / 4;
	constexpr float pi_mul_2 = pi * 2;

	template <size_t N>
	struct vec
	{
		vec() : vec(0) {}

		explicit vec(const float splat) : data{ splat } {}

		~vec() = default;

		float& operator[](size_t index)
		{
			return data[index];
		}

		const float& operator[](size_t index) const
		{
			return data[index];
		}

		float data[N];
	};

	template <>
	struct vec<2>
	{
		vec(float x, float y) : x(x), y(y) {}

		vec(float splat) : vec(splat, splat) {}

		vec() : vec(0) {}

		float& operator[](size_t index)
		{
			return data[index];
		}

		const float& operator[](size_t index) const
		{
			return data[index];
		}

		union
		{
			float data[2];

			struct
			{
				float x, y;
			};
		};
	};

	template <>
	struct vec<3>
	{
		vec(float x, float y, float z) : x(x), y(y), z(z) {}

		vec(const vec<2>& xy, float z) : vec(xy.x, xy.y, z) {}

		vec(float splat) : vec(splat, splat, splat) {}

		vec() : vec(0) {}

		~vec() = default;

		float& operator[](size_t index)
		{
			return data[index];
		}

		const float& operator[](size_t index) const
		{
			return data[index];
		}

		union
		{
			float data[3];

			struct
			{
				float x, y, z;
			};

			struct
			{
				float r, g, b;
			};

			vec<2> xy;
		};
	};

	namespace standard_basis
	{
		const vec<3> right{ 1, 0, 0 };
		const vec<3> up{ 0, 1, 0 };
		const vec<3> forward_lh{ 0, 0, 1 };
		const vec<3> forward_rh{ 0, 0, -1 };
	}

	template <>
	struct vec<4>
	{
		vec(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

		vec(const vec<2>& xy, float z, float w) : vec(xy.x, xy.y, z, w) {}

		vec(const vec<3>& xyz, float w) : vec(xyz.xy, xyz.z, w) {}

		explicit vec(float splat) : vec(splat, splat, splat, splat) {}

		vec() : vec(0, 0, 0, 0) {}

		float& operator[](size_t index)
		{
			return data[index];
		}

		const float& operator[](size_t index) const
		{
			return data[index];
		}

		union
		{
			float data[4];

			struct
			{
				float x, y, z, w;
			};

			struct
			{
				float r, g, b, a;
			};

			vec<3> rgb;

			vec<3> xyz;

			vec<3> xy;
		};
	};

	template <size_t N>
	vec<N> operator-(const vec<N>& a)
	{
		vec<N> result;

		for (size_t i = 0; i < N; ++i)
		{
			result[i] = -a[i];
		}

		return result;
	}

	template <size_t N>
	vec<N> operator+(const vec<N>& a, const vec<N>& b)
	{
		vec<N> result;

		for (size_t i = 0; i < N; ++i)
		{
			result[i] = a[i] + b[i];
		}

		return result;
	}

	template <size_t N>
	vec<N> operator+(const vec<N>& a, float b)
	{
		vec<N> result;

		for (size_t i = 0; i < N; ++i)
		{
			result[i] = a[i] + b;
		}

		return result;
	}

	template <size_t N>
	vec<N> operator+(float a, const vec<N>& b)
	{
		return b + a;
	}

	template <size_t N>
	vec<N> operator-(const vec<N>& a, const vec<N>& b)
	{
		return a + -b;
	}

	template <size_t N>
	vec<N> operator-(const vec<N>& a, float b)
	{
		return a + -b;
	}

	template <size_t N>
	vec<N> operator-(float a, const vec<N>& b)
	{
		return a + -b;
	}

	template <size_t N>
	vec<N>& operator+=(vec<N>& a, const vec<N>& b)
	{
		a = a + b;

		return a;
	}

	template <size_t N>
	vec<N>& operator+=(vec<N>& a, float b)
	{
		a = a + b;

		return a;
	}

	template <size_t N>
	vec<N>& operator-=(vec<N>& a, const vec<N>& b)
	{
		a = a - b;

		return a;
	}

	template <size_t N>
	vec<N>& operator-=(vec<N>& a, float b)
	{
		a = a - b;

		return a;
	}

	template <size_t N>
	vec<N> operator*(const vec<N>& a, const vec<N>& b)
	{
		vec<N> ret;

		for (size_t i = 0; i < N; ++i)
		{
			ret[i] = a[i] * b[i];
		}

		return ret;
	}

	template <size_t N>
	vec<N> operator*(const vec<N>& a, float b)
	{
		vec<N> ret;

		for (size_t i = 0; i < N; ++i)
		{
			ret[i] = a[i] * b;
		}

		return ret;
	}

	template <size_t N>
	vec<N> operator*(float a, const vec<N>& b)
	{
		return b * a;
	}

	template <size_t N>
	vec<N>& operator*=(vec<N>& a, float b)
	{
		a = a * b;

		return a;
	}

	template <size_t N>
	vec<N>& operator*=(vec<N>& a, const vec<N>& b)
	{
		a = a * b;

		return a;
	}

	template <size_t N>
	vec<N> operator/(const vec<N>& a, float b)
	{
		return a * (1 / b);
	}

	template <size_t N>
	vec<N> operator/(const vec<N>& a, const vec<N>& b)
	{
		vec<N> ret;

		for (size_t i = 0; i < N; ++i)
		{
			ret[i] = a[i] / b[i];
		}

		return ret;
	}

	template <size_t N>
	vec<N>& operator/=(vec<N>& a, float b)
	{
		a = a / b;

		return a;
	}

	template <size_t N>
	vec<N>& operator/=(vec<N>& a, const vec<N>& b)
	{
		a = a / b;

		return a;
	}

	template <size_t N>
	vec<N> normalize(const vec<N>& a)
	{
		return a / length(a);
	}

	template <size_t N>
	bool is_normalized(const vec<N>& a)
	{
		return abs(length(a) - 1.0f) < 0.001f;
	}

	template <size_t N>
	float dot(const vec<N>& a, const vec<N>& b)
	{
		float result = 0;

		for (size_t i = 0; i < N; ++i)
		{
			result += a[i] * b[i];
		}

		return result;
	}

	inline vec<2> cross(const vec<2>& a, const vec<2>& b)
	{
		return { a.x * b.y - a.y * b.x };
	}

	inline vec<3> cross(const vec<3>& a, const vec<3>& b)
	{
		return {
			a.y * b.z - a.z * b.y,
			a.z * b.x - a.x * b.z,
			a.x * b.y - a.y * b.x };
	}

	template <size_t N>
	vec<N> reflect(const vec<N>& a, const vec<N>& n)
	{
		return a - n * 2 * dot(a, n);
	}

	template <size_t N>
	math::vec<N> refract(const math::vec<N>& i, const math::vec<N>& n, float eta)
	{
		const float n_dot_i = math::dot(n, i);
		const float discr = 1.0f - eta * eta * (1 - n_dot_i * n_dot_i);
		if (discr < 0.0)
		{
			return 0;
		}

		return eta * i - (eta * n_dot_i + sqrt(discr)) * n;
	}

	template <size_t N>
	float length2(const vec<N>& a)
	{
		return dot(a, a);
	}

	template <size_t N>
	float length(const vec<N>& a)
	{
		return std::sqrt(dot(a, a));
	}

	template <size_t N>
	float distance(const vec<N>& a, const vec<N>& b)
	{
		return length(a - b);
	}

	template <size_t N>
	float distance2(const vec<N>& a, const vec<N>& b)
	{
		return length2(a - b);
	}

	template <size_t N>
	struct mat
	{
	public:
		mat()
		{
		}

		mat(std::initializer_list<float> values)
		{
			assert(values.size() == N * N);

			for (size_t row = 0; row < N; ++row)
			{
				for (size_t col = 0; col < N; ++col)
				{
					data[row][col] = *(values.begin() + (N * row + col));
				}
			}
		}

		mat(std::initializer_list<vec<N>> rows)
		{
			assert(rows.size() == N);

			for (size_t row = 0; row < N; ++row)
			{
				data[row] = *(rows.begin() + row);
			}
		}

		const vec<N>& operator[](size_t row) const
		{
			return data[row];
		}

		vec<N>& operator[](size_t row)
		{
			return data[row];
		}

		const vec<N>& row(size_t row) const
		{
			return data[row];
		}

		vec<N>& row(size_t row)
		{
			return data[row];
		}

		vec<N> data[N];
	};

	inline vec<3> get_translation(const mat<4>& m)
	{
		return m[3].xyz;
	}

	inline void set_translation(mat<4>& m, const vec<3>& translation)
	{
		m[3] = { translation.x, translation.y, translation.z, 1 };
	}

	inline vec<3> get_forward(const mat<4>& m)
	{
		return m[2].xyz;
	}

	inline vec<3> get_up(const mat<4>& m)
	{
		return m[1].xyz;
	}

	inline vec<3> get_right(const mat<4>& m)
	{
		return m[0].xyz;
	}

	inline void set_right(mat<4>& m, const vec<3>& right)
	{
		m[0] = { right.x, right.y, right.z, 0.0f };
	}

	template <size_t N>
	mat<N> operator*(const mat<N>& a, float b)
	{
		mat<N> ret;

		for (size_t row = 0; row < N; ++row)
		{
			ret[row] = a[row] * b;
		}

		return ret;
	}

	template <size_t N>
	mat<N>& operator*=(mat<N>& a, float b)
	{
		a = a * b;

		return a;
	}

	template <size_t N>
	mat<N> operator/(const mat<N>& a, float b)
	{
		return a * (1 / b);
	}

	template <size_t N>
	mat<N>& operator/=(mat<N>& a, float b)
	{
		a = a / b;

		return a;
	}

	template <size_t N>
	mat<N> create_identity()
	{
		mat<N> ret;

		for (size_t i = 0; i < N; ++i)
		{
			ret[i][i] = 1;
		}

		return ret;
	}

	inline mat<4> create_translation(const vec<3>& translation)
	{
		mat<4> ret = create_identity<4>();
		ret[3][0] = translation.x;
		ret[3][1] = translation.y;
		ret[3][2] = translation.z;
		ret[3][3] = 1.0f;

		return ret;
	}

	inline mat<4> create_look_at_lh(
		const vec<3>& at,
		const vec<3>& eye,
		const vec<3>& up)
	{
		const vec<3> forward = normalize(at - eye); // z
		const vec<3> right = normalize(cross(up, forward)); // x
		const vec<3> new_up = cross(forward, right); // y
		return{
			vec<4>(right.x, new_up.x, forward.x, 0),
			vec<4>(right.y, new_up.y, forward.y, 0),
			vec<4>(right.z, new_up.z, forward.z, 0),
			vec<4>({ -dot(right, eye), -dot(up, eye), -dot(forward, eye) }, 1) };
	}

	inline mat<4> create_look_at_rh(
		const vec<3>& at,
		const vec<3>& eye,
		const vec<3>& up)
	{
		const vec<3> forward = normalize(eye - at);
		const vec<3> right = normalize(cross(up, forward));
		const vec<3> new_up = cross(forward, right);
		return{
			vec<4>(right.x, new_up.x, forward.x, 0),
			vec<4>(right.y, new_up.y, forward.y, 0),
			vec<4>(right.z, new_up.z, forward.z, 0),
			vec<4>({ dot(right, eye), dot(up, eye), dot(forward, eye) }, 1) };
	}

	inline mat<4> create_perspective_fov_rh(
		float fovy,
		float aspect,
		float znear,
		float zfar)
	{
		// The width and height of the view volume at the near plane
		const float height = 1 / std::tan(fovy / 2);
		const float width = height / aspect;
		const float zdist = (znear - zfar);
		const float zfar_per_zdist = zfar / zdist;
		return {
			{ width, 0, 0, 0 },
			{ 0, height, 0, 0 },
			{ 0, 0, zfar_per_zdist, -1 },
			{ 0, 0, 2.0f * znear * zfar_per_zdist, 0 }
		};
	}

	inline mat<4> create_perspective_fov_lh(
		float fovy,
		float aspect,
		float znear,
		float zfar)
	{
		// The width and height of the view volume at the near plane
		const float height = 1 / std::tan(fovy / 2);
		const float width = height / aspect;
		const float zdist = (znear - zfar);
		const float zfar_per_zdist = zfar / zdist;
		return {
			{ width, 0, 0, 0 },
			{ 0, height, 0, 0 },
			{ 0, 0, -zfar_per_zdist, 1 },
			{ 0, 0, 2.0f * znear * zfar_per_zdist, 0 }
		};
	}

	// matrix representing counter-clockwise rotation about the X axis
	inline mat<4> create_rotation_x(float radians)
	{
		return {
			{ 1.0f, 0.0f, 0.0f, 0.0f },
			{ 0.0f, cos(radians), sin(radians), 0.0f },
			{ 0.0f, -sinf(radians), cosf(radians), 0.0f },
			{ 0.0f, 0.0f, 0.0f, 1.0f },
		};
	}

	// matrix representing counter-clockwise rotation about the Y axis
	inline mat<4> create_rotation_y(float radians)
	{
		return {
			{ cos(radians), 0.0f, sin(radians), 0.0f },
			{ 0.0f, 1.0f, 0.0f, 0.0f },
			{ -sinf(radians), 0.0f, cosf(radians), 0.0f },
			{ 0.0f, 0.0f, 0.0f, 1.0f },
		};
	}

	// matrix representing counter-clockwise rotation about the Z axis
	inline mat<4> create_rotation_z(float radians)
	{
		return {
			{ cos(radians), -sin(radians), 0.0f, 0.0f },
			{ sin(radians), cos(radians), 0.0f, 0.0f },
			{ 0.0f, 0.0f, 1.0f, 0.0f },
			{ 0.0f, 0.0f, 0.0f, 1.0f },
		};
	}

	mat<4> create_scale(float scale)
	{
		mat<4> ret = create_identity<4>();

		for (size_t i = 0; i < 3; ++i)
		{
			ret[i][i] = scale;
		}

		return ret;
	}

	mat<4> create_scale(const math::vec<3>& scale)
	{
		mat<4> ret = create_identity<4>();

		for (size_t i = 0; i < 3; ++i)
		{
			ret[i][i] = scale[i];
		}

		return ret;
	}

	template <size_t N>
	mat<N> multiply(const mat<N>& a, const mat<N>& b)
	{
		mat<N> ret;

		for (size_t row = 0; row < N; ++row)
		{
			for (size_t col = 0; col < N; ++col)
			{
				for (size_t i = 0; i < N; ++i)
				{
					ret[row][col] += a[row][i] * b[i][col];
				}
			}
		}

		return ret;
	}

	template <size_t N>
	mat<N> operator*(const mat<N>& a, const mat<N>& b)
	{
		return multiply(a, b);
	}

	template <size_t N>
	mat<N> transpose(const mat<N>& a)
	{
		mat<N> ret;

		for (size_t row = 0; row < N; ++row)
		{
			for (size_t col = 0; col < N; ++col)
			{
				ret[row][col] += a[col][row];
			}
		}

		return ret;
	}

	// Remove the selected column and row from the matrix
	template <size_t N>
	mat<N - 1> minor(const mat<N>& a, size_t row, size_t col)
	{
		mat<N - 1> ret;

		size_t dest_row = 0;

		for (size_t source_row = 0; source_row < N; ++source_row)
		{
			if (source_row == row)
			{
				continue;
			}

			size_t dest_col = 0;

			for (size_t source_col = 0; source_col < N; ++source_col)
			{
				if (source_col == col)
				{
					continue;
				}

				ret[dest_row][dest_col] = a[source_row][source_col];

				++dest_col;
			}

			++dest_row;
		}

		return ret;
	}

	template <size_t N>
	float determinant(const mat<N>& a)
	{
		float ret = 0;

		for (size_t col = 0; col < N; ++col)
		{
			ret += ((col % 2) ? -1 : 1) * a[0][col] * determinant(minor(a, 0, col));
		}

		return ret;
	}

	template <>
	inline float determinant(const mat<1>& a)
	{
		return a[0][0];
	}

	template <>
	inline float determinant(const mat<2>& a)
	{
		return a[0][0] * a[1][1] - a[1][0] * a[0][1];
	}

	template<size_t N>
	mat<N> adjoint(const mat<N>& a)
	{
		mat<N> ret;

		for (size_t row = 0; row < N; ++row)
		{
			for (size_t col = 0; col < N; ++col)
			{
				ret[row][col] = (((row + col) % 2) ? -1 : 1) * determinant(minor(a, col, row));
			}
		}
		return ret;
	}

	template<size_t N>
	mat<N> inverse(const mat<N>& a)
	{
		return adjoint(a) / determinant(a);
	}

	template<>
	inline mat<1> inverse(const mat<1>& a)
	{
		return a;
	}

	inline vec<3> transform_point(const mat<4>& m, const vec<3>& point)
	{
		vec<4> ret(
			m.row(0) * point.x +
			m.row(1) * point.y +
			m.row(2) * point.z +
			m.row(3));

		float w = ret.w;

		ret /= w;

		return ret.xyz;
	}

	inline vec<3> transform_vector(const mat<4>& m, const vec<3>& vector)
	{
		vec<4> ret(
			m.row(0) * vector.x +
			m.row(1) * vector.y +
			m.row(2) * vector.z);

		return ret.xyz;
	}

	template <typename T>
	T square(const T& a)
	{
		return a * a;
	}

	template<size_t N>
	vec<N> saturate(vec<N> val, vec<N> min = 0, vec<N> max = 1)
	{
		vec<N> result;
		for (size_t i = 0; i < N; ++i)
		{
			result[i] = std::min(std::max(val[i], min[i]), max[i]);
		}
		return result;
	}

	template<typename T>
	T lerp(T a, T b, T t)
	{
		return (1 - t) * a + t * b;
	}

	template<size_t N>
	vec<N> step(const vec<N>& x, const vec<N>& edge)
	{
		vec<N> result(1.0f);

		for (size_t i = 0; i < N; ++i)
		{
			if (x[i] < edge[i])
			{
				result[i] = 0.0f;
			}
		}

		return result;
	}

	inline float remap(float original_value, float original_min, float original_max, float new_min, float new_max)
	{
		return new_min + (((original_value - original_min) / (original_max - original_min)) * (new_max - new_min));
	}

	inline void orthonormal_basis(const math::vec<3>& w, math::vec<3>* v, math::vec<3>* u)
	{
		if (std::abs(w.x) > std::abs(w.y)) 
		{
			const float inverse_length = 1.0f / std::sqrt(w.x * w.x + w.z * w.z);
			*u = { w.z * inverse_length, 0.0f, -w.x * inverse_length };
		}
		else 
		{
			const float inverse_length = 1.0f / std::sqrt(w.y * w.y + w.z * w.z);
			*u = { 0.0f, w.z * inverse_length, -w.y * inverse_length };
		}
		*v = cross(*u, w);
	}
}