#pragma once

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cassert>
#include <cmath>
#include <vector>
#include <string>
#include <memory>
#include <wrl.h>              // Microsoft::WRL::ComPtr

// DirectX 12 & DXGI
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>

// DX helper
#include "d3dx12.h"

#include <DirectXMath.h>

// ImGui
#include "imgui.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"


using Microsoft::WRL::ComPtr;
