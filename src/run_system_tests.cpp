#include "SystemChecks.h"
#include <iostream>

int main() {
    std::cout << "Running System Checks...\n" << std::endl;

    auto cudaResult = SystemChecks::checkCudaAvailable();
    std::cout << "[CUDA CHECK] " << (cudaResult.passed ? "PASS" : "FAIL") 
              << ": " << cudaResult.message << std::endl;

    auto colmapResult = SystemChecks::checkColmapAvailable();
    std::cout << "[COLMAP BINARY] " << (colmapResult.passed ? "PASS" : "FAIL") 
              << ": " << colmapResult.message << std::endl;

    SystemChecks::CheckResult colmapCudaResult = {false, "Skipped because COLMAP binary not found."};
    
    if (colmapResult.passed) {
        colmapCudaResult = SystemChecks::checkColmapCudaSupport();
        std::cout << "[COLMAP CUDA] " << (colmapCudaResult.passed ? "PASS" : "FAIL") 
                  << ": " << colmapCudaResult.message << std::endl;
    }

    bool allPassed = cudaResult.passed && colmapResult.passed && colmapCudaResult.passed;

    if (allPassed) {
        std::cout << "\nALL SYSTEM CHECKS PASSED." << std::endl;
        return 0;
    } else {
        std::cout << "\nSOME SYSTEM CHECKS FAILED." << std::endl;
        return 1;
    }
}

