#pragma once

#include <windows.h>

struct sp_window_desc
{
	int width, height;
};

struct sp_window
{
	void* _handle;
};

typedef void(*sp_window_event_key_press_cb)(void* user_data, char key);
typedef void(*sp_window_event_mouse_move_cb)(void* user_data, int x, int y);
typedef void(*sp_window_event_resize_cb)(void* user_data, int width, int height);
typedef void(*sp_window_event_char_cb)(void* user_data, char key);

namespace detail
{
	struct sp_window_event_callbacks
	{
		sp_window_event_key_press_cb on_key_down = nullptr;
		void* on_key_down_user_data = nullptr;
		sp_window_event_key_press_cb on_key_up = nullptr;
		void* on_key_up_user_data = nullptr;
		sp_window_event_mouse_move_cb on_mouse_move = nullptr;
		void* on_mouse_move_user_data = nullptr;
		sp_window_event_resize_cb on_resize = nullptr;
		void* on_resize_user_data = nullptr;
		sp_window_event_char_cb on_char = nullptr;
		void* on_char_user_data = nullptr;
	};

	sp_window_event_callbacks* sp_window_get_event_callbacks()
	{
		static sp_window_event_callbacks instance;
		return &instance;
	}
};

void sp_window_event_set_key_down_callback(sp_window_event_key_press_cb on_key_down, void* user_data)
{
	detail::sp_window_get_event_callbacks()->on_key_down = on_key_down;
	detail::sp_window_get_event_callbacks()->on_key_down_user_data = user_data;
}

void sp_window_event_set_key_up_callback(sp_window_event_key_press_cb on_key_up, void* user_data)
{
	detail::sp_window_get_event_callbacks()->on_key_up = on_key_up;
	detail::sp_window_get_event_callbacks()->on_key_up_user_data = user_data;
}

void sp_window_event_set_char_callback(sp_window_event_char_cb on_char, void* user_data)
{
	detail::sp_window_get_event_callbacks()->on_char = on_char;
	detail::sp_window_get_event_callbacks()->on_char_user_data = user_data;
}

void sp_window_event_set_mouse_move_callback(sp_window_event_mouse_move_cb on_mouse_move, void* user_data)
{
	detail::sp_window_get_event_callbacks()->on_mouse_move = on_mouse_move;
	detail::sp_window_get_event_callbacks()->on_mouse_move_user_data = user_data;
}

void sp_window_event_set_resize_callback(sp_window_event_resize_cb on_resize, void* user_data)
{
	detail::sp_window_get_event_callbacks()->on_resize = on_resize;
	detail::sp_window_get_event_callbacks()->on_resize_user_data = user_data;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_SIZE:
	{
		const detail::sp_window_event_callbacks* callbacks = detail::sp_window_get_event_callbacks();
		if (callbacks->on_resize)
		{
			(*callbacks->on_resize)(callbacks->on_resize_user_data, LOWORD(lParam), HIWORD(lParam));
			return 0;
		}
		break;
	}
	case WM_KEYDOWN:
	{
		const detail::sp_window_event_callbacks* callbacks = detail::sp_window_get_event_callbacks();
		if (callbacks->on_key_down)
		{
			(*callbacks->on_key_down)(callbacks->on_key_down_user_data, static_cast<char>(wParam));
			return 0;
		}
		break;
	}
	case WM_KEYUP:
	{
		const detail::sp_window_event_callbacks* callbacks = detail::sp_window_get_event_callbacks();
		if (callbacks->on_key_up)
		{
			(*callbacks->on_key_up)(callbacks->on_key_up_user_data, static_cast<char>(wParam));
			return 0;
		}
		break;
	}
	case WM_CHAR:
	{
		const detail::sp_window_event_callbacks* callbacks = detail::sp_window_get_event_callbacks();
		if (callbacks->on_char)
		{
			(*callbacks->on_char)(callbacks->on_char, static_cast<char>(wParam));
			return 0;
		}
		break;
	}
	case WM_LBUTTONDOWN:
	{
		const detail::sp_window_event_callbacks* callbacks = detail::sp_window_get_event_callbacks();
		if (callbacks->on_key_down)
		{
			(*callbacks->on_key_down)(callbacks->on_key_down_user_data, VK_LBUTTON);
			return 0;
		}
		break;
	}
	case WM_LBUTTONUP:
	{
		const detail::sp_window_event_callbacks* callbacks = detail::sp_window_get_event_callbacks();
		if (callbacks->on_key_up)
		{
			(*callbacks->on_key_up)(callbacks->on_key_up_user_data, VK_LBUTTON);
			return 0;
		}
		break;
	}
	case WM_RBUTTONDOWN:
	{
		const detail::sp_window_event_callbacks* callbacks = detail::sp_window_get_event_callbacks();
		if (callbacks->on_key_down)
		{
			(*callbacks->on_key_down)(callbacks->on_key_down_user_data, VK_RBUTTON);
			return 0;
		}
		break;
	}
	case WM_RBUTTONUP:
	{
		const detail::sp_window_event_callbacks* callbacks = detail::sp_window_get_event_callbacks();
		if (callbacks->on_key_up)
		{
			(*callbacks->on_key_up)(callbacks->on_key_up_user_data, VK_RBUTTON);
			return 0;
		}
		break;
	}
	case WM_MOUSEMOVE:
	{
		const detail::sp_window_event_callbacks* callbacks = detail::sp_window_get_event_callbacks();
		if (callbacks->on_mouse_move)
		{
			(*callbacks->on_mouse_move)(callbacks->on_mouse_move_user_data, LOWORD(lParam), HIWORD(lParam));
			return 0;
		}
		break;
	}
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

sp_window sp_window_create(const char* name, const sp_window_desc& desc)
{
	HINSTANCE hInst = GetModuleHandle(0);

	// Initialize the window class.
	WNDCLASSEX wc;
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInst;
	wc.hIcon = LoadIcon(hInst, IDI_APPLICATION);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName = nullptr;
	wc.lpszClassName = name;
	wc.hIconSm = LoadIcon(hInst, IDI_APPLICATION);
	RegisterClassEx(&wc);

	RECT rc = { 0, 0, static_cast<LONG>(desc.width), static_cast<LONG>(desc.height) };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

	// Create the window and store a handle to it.
	HWND hWnd = CreateWindow(
		wc.lpszClassName,
		name,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		rc.right - rc.left,
		rc.bottom - rc.top,
		nullptr,
		nullptr,
		hInst,
		nullptr);

	ShowWindow(hWnd, SW_SHOWDEFAULT);
	UpdateWindow(hWnd);

	return { static_cast<void*>(hWnd) };
}

bool sp_window_poll()
{
	MSG msg;

	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);

		if (msg.message == WM_QUIT)
		{
			return false;
		}
	}

	return true;
}