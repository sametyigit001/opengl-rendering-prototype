#include "winHost.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

struct winHost::impl {
	HWND hwnd = nullptr;

	static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
		if (m == WM_ERASEBKGND) return 1;
		return DefWindowProcW(h, m, w, l);
	}
};

winHost::winHost() : m_impl(new impl()){}
winHost::~winHost() {
	if (m_impl->hwnd)DestroyWindow(m_impl->hwnd);
	delete m_impl;
}

bool winHost::create(void* parent, uint32_t w, uint32_t h) {
	HINSTANCE inst = GetModuleHandleW(nullptr);
	WNDCLASSEXW wc = { sizeof(WNDCLASSEXW), CS_OWNDC, impl::WndProc, 0, 0, inst, nullptr, nullptr, nullptr, nullptr, L"Viewport", nullptr };
	wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	RegisterClassExW(&wc);
	
	m_impl->hwnd = CreateWindowExW(0, L"Viewport", nullptr,
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
		0, 0, w, h, (HWND)parent, nullptr, inst, nullptr);

	return m_impl->hwnd != nullptr;
}

void winHost::resize(uint32_t w, uint32_t h) {
	SetWindowPos(m_impl->hwnd, nullptr, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void* winHost::getHandle() const { return m_impl->hwnd; }
