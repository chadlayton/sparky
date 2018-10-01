#pragma once

#include <windows.h>

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_SIZE:
		//graphics::resize((UINT)(UINT64)lParam & 0xFFFF, (UINT)(UINT64)lParam >> 16);
		break;

	case WM_KEYDOWN:
		//pSample->OnKeyDown(static_cast<UINT8>(wParam));
		return 0;

	case WM_KEYUP:
		//pSample->OnKeyUp(static_cast<UINT8>(wParam));
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

struct sp_window
{
	void* _handle;
};

sp_window sp_window_create(const WCHAR* name, int width, int height)
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

	RECT rc = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
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

	return { static_cast<void*>(hWnd) };
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