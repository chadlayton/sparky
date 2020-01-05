#pragma once

void* sp_image_create_from_file(const char* filename)
{
	int image_width, image_height, image_channels;
	stbi_uc* image_data = stbi_load(filename, &image_width, &image_height, &image_channels, STBI_rgb_alpha);
	assert(image_data);

	return image_data;
}

void* sp_image_create_from_file_hdr(const char* filename)
{
	int image_width, image_height, image_channels;
	float* image_data = stbi_loadf(filename, &image_width, &image_height, &image_channels, STBI_rgb_alpha);
	assert(image_data);

	return image_data;
}

void* sp_image_volume_create_from_directory(const char* dirname, const char* format, int size)
{
	const int volume_width = size;
	const int volume_height = size;
	const int volume_depth = size;

	const int image_slice_size_bytes = volume_width * volume_height * 4; // TODO: Assuming 4bpp
	const int image_volume_size_bytes = image_slice_size_bytes * volume_depth;

	uint8_t* image_volume_data = static_cast<uint8_t*>(malloc(image_volume_size_bytes));
	assert(image_volume_data);

	for (int i = 0; i < volume_depth; ++i)
	{
		char filename[512];
		int len = snprintf(filename, 511, "%s/", dirname);
		snprintf(filename + len, 511 - len, format, i);

		int image_slice_width, image_slice_height, image_slice_channels;
		stbi_uc* image_slice_data = stbi_load(filename, &image_slice_width, &image_slice_height, &image_slice_channels, STBI_rgb_alpha);
		assert(image_slice_data);
		assert(image_slice_width == volume_width);
		assert(image_slice_height == volume_height);

		memcpy(image_volume_data + (image_slice_size_bytes * i), image_slice_data, image_slice_size_bytes);

		stbi_image_free(image_slice_data);
	}

	return image_volume_data;
}