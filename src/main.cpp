#include "XrWGPUBridge.h"

#include <iostream>
#include <exception>

int main() {
    try {
        std::cout << "Starting VR Application" << std::endl;
        XrWGPUBridge vrBridge;
        vrBridge.Initialize();
        bool isRunning = true;
        while(isRunning) {
            // TODO: Render Loop
            isRunning = vrBridge.RenderFrame();
        }
        std::cout << "Shutting down gracefully" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return -1;
    }
    return 0;
}
