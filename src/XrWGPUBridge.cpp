#include "XrWGPUBridge.h"
#include <iostream>
#include <cassert>

XrWGPUBridge::XrWGPUBridge() {}

XrWGPUBridge::~XrWGPUBridge() {

}

void XrWGPUBridge::Initialize() {
    InitOpenXR();
    InitVulkanViaOpenXR();
    InitDawnFromVulkan();
    CreateSwapchainsAndRenderTargets();
    SetupVulkanBlitCommand();
}

