#include "pch.h"

#include "main.h"

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool EngineD3D12::initialize(HWND hwnd_, uint32_t width_, uint32_t height_)
{
	this->hwnd = hwnd_;
	this->width = width_;
	this->height = height_;

	try {
		create_device();
		create_command_objects();
		create_swapchain();
		create_render_targets();

		if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
			throw std::runtime_error("Failed to create fence.");

		fence_value = 1;
		fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (!fence_event) {
			throw std::runtime_error("Failed to create fence event.");
		}

		return true;
	}
	catch (const std::exception& e) {
		MessageBoxA(hwnd, e.what(), "D3D12 Initialization Error", MB_ICONERROR);
		return false;
	}
}

void EngineD3D12::render_frame()
{
	command_allocator[frame_index]->Reset();
	command_list->Reset(command_allocator[frame_index].Get(), nullptr);

	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(render_targets[frame_index].Get(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET);
	command_list->ResourceBarrier(1, &barrier);

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(
		rtv_heap->GetCPUDescriptorHandleForHeapStart(),
		frame_index,
		rtv_descriptor_size);

	command_list->OMSetRenderTargets(1, &rtv_handle, FALSE, nullptr);

	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	command_list->ClearRenderTargetView(rtv_handle, clearColor, 0, nullptr);

	barrier = CD3DX12_RESOURCE_BARRIER::Transition(render_targets[frame_index].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

	command_list->ResourceBarrier(1, &barrier);

	command_list->Close();
	ID3D12CommandList* command_lists[] = { command_list.Get() };
	command_queue->ExecuteCommandLists(_countof(command_lists), command_lists);

	swapchain->Present(1, 0);

	wait_for_gpu();
}

void EngineD3D12::cleanup()
{
	wait_for_gpu();
	CloseHandle(fence_event);
}

void EngineD3D12::create_device()
{
	ComPtr<IDXGIFactory4> factory;
	if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
		throw std::runtime_error("Failed to create DXGI factory.");

	if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)))) {
		throw std::runtime_error("Failed to create D3D12 device.");
	}
}

// setup queue, allocators and command list 
// https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nn-d3d12-id3d12commandqueue
void EngineD3D12::create_command_objects()
{
	D3D12_COMMAND_QUEUE_DESC queue_desc = {};
	queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	if (FAILED(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&command_queue))))
		throw std::runtime_error("Failed to create command queue.");

	for (UINT i = 0; i < frame_count; ++i) {
		if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocator[i]))))
			throw std::runtime_error("Failed to create command allocator.");
	}

	// command list
	if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocator[0].Get(), nullptr, IID_PPV_ARGS(&command_list)))) {
		throw std::runtime_error("Failed to create command list.");
	}

	command_list->Close();
}

void EngineD3D12::create_swapchain()
{
	ComPtr<IDXGIFactory4> factory;
	CreateDXGIFactory1(IID_PPV_ARGS(&factory));

	DXGI_SWAP_CHAIN_DESC1 swapchain_desc = {};
	swapchain_desc.BufferCount = frame_count;
	swapchain_desc.Width = width;
	swapchain_desc.Height = height;
	swapchain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapchain_desc.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> temp_swapchain;
	if (FAILED(factory->CreateSwapChainForHwnd(command_queue.Get(), hwnd, &swapchain_desc, nullptr, nullptr, &temp_swapchain))) {
		throw std::runtime_error("Failed to create swapchain.");
	}

	temp_swapchain.As(&swapchain);
	frame_index = swapchain->GetCurrentBackBufferIndex();
}

void EngineD3D12::create_render_targets()
{
	// render target descriptor heap
	D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
	heap_desc.NumDescriptors = frame_count;
	heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	if (FAILED(device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&rtv_heap)))) {
		throw std::runtime_error("Failed to create RTV heap.");
	}
	rtv_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(rtv_heap->GetCPUDescriptorHandleForHeapStart());

	for (UINT i = 0; i < frame_count; ++i) {
		if (FAILED(swapchain->GetBuffer(i, IID_PPV_ARGS(&render_targets[i]))))
			throw std::runtime_error("Failed to get swapchain buffer.");

		device->CreateRenderTargetView(render_targets[i].Get(), nullptr, rtv_handle);
		rtv_handle.Offset(1, rtv_descriptor_size);
	}
}

void EngineD3D12::wait_for_gpu()
{
	const UINT64 currentFence = fence_value;
	if (FAILED(command_queue->Signal(fence.Get(), currentFence)))
		throw std::runtime_error("Failed to signal fence.");

	fence_value++;

	if (fence->GetCompletedValue() < currentFence) {
		if (FAILED(fence->SetEventOnCompletion(currentFence, fence_event)))
			throw std::runtime_error("Failed to set fence event.");

		WaitForSingleObject(fence_event, INFINITE);
	}

	frame_index = swapchain->GetCurrentBackBufferIndex();
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
	const wchar_t* className = L"DX12WindowClass";
	const uint32_t window_width = 1280;
	const uint32_t window_height = 720;

	WNDCLASSEX wc = {};
	wc.cbSize = sizeof(wc);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.lpszClassName = className;

	RegisterClassEx(&wc);

	RECT windowRect = { 0, 0, (LONG)window_width, (LONG)window_height };
	AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

	// setup window title 
	std::wstringstream wss;
	wss << L"DX12 Renderer v"
		<< _VERSION_NO_MAJOR << L"." << _VERSION_NO_MINOR
		<< L" [" << _BUILD_TYPE << L"]"
		<< L" - (" << __DATE__ << L" " << __TIME__ << ")";

	std::wstring windowTitle = wss.str();

	HWND hwnd = CreateWindowEx(
		0,
		className,
		windowTitle.c_str(), // const wchar_t*
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top,
		nullptr, nullptr,
		hInstance,
		nullptr
	);

	ShowWindow(hwnd, nCmdShow);

	// to-do: provide abstractions for these 
	// new instance of renderer 
	EngineD3D12 renderer;
	if (!renderer.initialize(hwnd, window_width, window_height)) {
		return -1;
	}

	// main render loop 
	MSG msg = {};
	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
			renderer.render_frame();
		}
	}

	renderer.cleanup();
	return 0;
}
