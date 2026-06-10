#include <cstring>
#include <openxr/openxr.h>

#include <iostream>
#include <ostream>
#include <vector>

#define XR_CHECK(res, msg)                                                      \
    if (XR_FAILED(res)) {                                                       \
        std::cerr << "[ERROR] " << msg << " (Code: " << res << ")" << std::endl; \
        exit(1);                                                                \
    }

int main() {
    std::cout << "[INFO] Initializing OpenXR Demo..." << std::endl;

    // Instance creation
    XrApplicationInfo appInfo = {};
    strcpy(appInfo.applicationName, "OpenXR_Demo");
    appInfo.applicationVersion = 1;
    strcpy(appInfo.engineName, "None");
    appInfo.engineVersion = 1;
    appInfo.apiVersion = XR_CURRENT_API_VERSION;
    // Here goes extension like XR_KHR_vulkan_enable2 to use backend
    XrInstanceCreateInfo instanceCreateInfo = {XR_TYPE_INSTANCE_CREATE_INFO};
    instanceCreateInfo.applicationInfo = appInfo;
    instanceCreateInfo.enabledExtensionCount = 0;
    instanceCreateInfo.enabledExtensionNames = nullptr;
    XrInstance instance;
    XR_CHECK(xrCreateInstance(&instanceCreateInfo, &instance), "Cannot create instance. Possibly runtime not found.");
    std::cout << "[INFO] XrInstance successfully created" << std::endl;

    // System retrieval (HMD)
    XrSystemGetInfo systemInfo = {XR_TYPE_SYSTEM_GET_INFO};
    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrSystemId systemId;
    XR_CHECK(xrGetSystem(instance, &systemInfo, &systemId), "Cannot find HMD");

    // Session creation
    // TODO: make middleman for WebGPU...
    XrSessionCreateInfo sessionCreateInfo = {XR_TYPE_SESSION_CREATE_INFO};
    sessionCreateInfo.systemId = systemId;
    // sessionCreateInfo.next = &graphicsBinding // <-- Here WebGPU joins the chat
    XrSession session;
    XR_CHECK(xrCreateSession(instance, &sessionCreateInfo, &session), "Cannot create session");
    std::cout << "[INFO] XrSession created" << std::endl;

    // Main loop
    std::cout << "[INFO] Main loop starting..." << std::endl;
    bool isRunning = true;
    while (isRunning) {
        XrEventDataBuffer eventData = {XR_TYPE_EVENT_DATA_BUFFER};
        XrResult pollResult = xrPollEvent(instance, &eventData);
        if (pollResult == XR_SUCCESS) {
            switch (eventData.type) {
                case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                    auto *stateEvent = reinterpret_cast<XrEventDataSessionStateChanged*>(&eventData);
                    std::cout << "New session state: " << stateEvent->state << std::endl;
                    if (stateEvent->state == XR_SESSION_STATE_EXITING || stateEvent->state == XR_SESSION_STATE_LOSS_PENDING) {
                        isRunning = false;
                    }
                    break;
                }
                case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
                    std::cout << "[WARN] VR instance loss imminent!" << std::endl;
                    isRunning = false;
                    break;
                default:
                    break;
            }
        }
    }

    // Cleanup
    xrDestroyInstance(instance);
    std::cout << "[INFO] Clean exit completed." << std::endl;
    return 0;
}
