#pragma once

#include <windows.h>

typedef void(*sp_window_keyboard_event_cb)(char key);

struct sp_window_event_callbacks
{
	sp_window_keyboard_event_cb keyboard;
};

struct sp_window_desc
{
	int width, height;
	sp_window_event_callbacks callbacks;
};

struct sp_window
{
	void* _handle;
	sp_window_event_callbacks* _callbacks;
};

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_NCCREATE:
	{
		CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
		sp_window_event_callbacks* callbacks = reinterpret_cast<sp_window_event_callbacks*>(cs->lpCreateParams);
		SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(callbacks));
		break;
	}
	case WM_SIZE:
		//graphics::resize((UINT)(UINT64)lParam & 0xFFFF, (UINT)(UINT64)lParam >> 16);
		break;

	case WM_KEYDOWN:
		if (const sp_window_event_callbacks* callbacks = reinterpret_cast<sp_window_event_callbacks*>(GetWindowLongPtr(hWnd, GWLP_USERDATA)))
		{
			(*callbacks->keyboard)(static_cast<char>(wParam));
		}
		break;
	case WM_KEYUP:
		//pSample->OnKeyUp(static_cast<UINT8>(wParam));
		break;

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

	sp_window_event_callbacks* callbacks = new sp_window_event_callbacks(desc.callbacks);

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
		callbacks);

	ShowWindow(hWnd, SW_SHOWDEFAULT);
	UpdateWindow(hWnd);

	return { static_cast<void*>(hWnd), callbacks };
}

void sp_window_destroy(sp_window* window)
{
	delete window->_callbacks;
	window->_callbacks = nullptr;
}

bool sp_window_poll()
{
	MSG msg;

	if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return msg.message != WM_QUIT;
}