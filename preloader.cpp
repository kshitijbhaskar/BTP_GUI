#include <windows.h>
#include <iostream>
#include <string>

// Check if a DLL can be loaded
bool loadDLL(const std::string& dllName) {
    HMODULE hModule = LoadLibraryA(dllName.c_str());
    if (hModule) {
        std::cout << "Successfully loaded: " << dllName << std::endl;
        // Check for the problematic function
        if (dllName.find("spatialite") != std::string::npos) {
            FARPROC procAddr = GetProcAddress(hModule, "xmlNanoHTTPCleanup");
            if (procAddr) {
                std::cout << "  - Found xmlNanoHTTPCleanup function!" << std::endl;
            } else {
                std::cout << "  - Could not find xmlNanoHTTPCleanup function. Error: " 
                          << GetLastError() << std::endl;
            }
        }
        FreeLibrary(hModule);
        return true;
    } else {
        DWORD error = GetLastError();
        std::cout << "Failed to load: " << dllName << ". Error: " << error << std::endl;
        return false;
    }
}

int main() {
    std::cout << "=== DLL Dependency Checker ===" << std::endl;
    
    // First try to load libxml2.dll
    loadDLL("C:\\OSGeo4W\\bin\\libxml2.dll");
    
    // Then try to load spatialite.dll
    loadDLL("C:\\OSGeo4W\\bin\\spatialite.dll");
    
    // Try loading the GDAL DLL
    loadDLL("C:\\OSGeo4W\\bin\\gdal.dll");
    
    // If that didn't work, try the full path to gdal.dll
    loadDLL("C:\\OSGeo4W\\apps\\gdal-dev\\bin\\gdal.dll");
    
    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get();
    return 0;
} 