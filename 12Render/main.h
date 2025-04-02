#include "pch.h"
#pragma once

#define _VERSION_NO_MAJOR 0
#define _VERSION_NO_MINOR 1
#define _BUILD_TYPE "internal" 

// using ComPtrs for automatic lifetime management
// RAII style cleanup, reduces boilerplate 
// raw pointers cause leaks or crashes
class EngineD3D12 {
public:
	bool initialize(HWND hwnd_, uint32_t width_, uint32_t height_);
	void render_frame();
	void cleanup();

private:
	void create_device();
	void create_command_objects();
	void create_swapchain();
	void create_render_targets();
	void wait_for_gpu();
	
	static const UINT frame_count = 2;

	ComPtr<ID3D12Device> device;
	ComPtr<ID3D12CommandQueue> command_queue;
	ComPtr<IDXGISwapChain3> swapchain;
	ComPtr<ID3D12CommandAllocator> command_allocator[frame_count];
	ComPtr<ID3D12GraphicsCommandList> command_list;

	ComPtr<ID3D12DescriptorHeap> rtv_heap;
	ComPtr<ID3D12Resource> render_targets[frame_count];
	UINT rtv_descriptor_size = 0;

	// Sync
	UINT frame_index = 0;
	ComPtr<ID3D12Fence> fence;
	HANDLE fence_event = nullptr;
	UINT64 fence_value = 0;

	HWND hwnd;

	uint32_t width = 0, height = 0;
};