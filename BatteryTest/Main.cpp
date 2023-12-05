#include <iostream>
#include <Windows.h>
#include <wbemcli.h>
#include <wbemprov.h>

using namespace std;


int main() {
    HRESULT hr = CoInitializeEx (0, COINIT_MULTITHREADED);

    hr = CoInitializeSecurity (NULL, 
        -1, 
        NULL, 
        NULL,
        RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, 
        EOAC_DYNAMIC_CLOAKING, 
        NULL);
    if (FAILED(hr)) {
        CoUninitialize();
        cout << "Failed to initialize security. Error code = 0x" << hex << hr << endl;
        return -1;
    }

    // TODO..
}
