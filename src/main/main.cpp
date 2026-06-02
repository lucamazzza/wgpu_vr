#include <openxr/openxr.h>
#include <wgpu.h>

#include <cstdio>
#include <iostream>

namespace {

void copyString(char* destination, size_t destinationSize, const char* source) {
    if (destinationSize == 0) {
        return;
    }
    std::snprintf(destination, destinationSize, "%s", source);
}

} // namespace

int main() {
    return 0;
}
