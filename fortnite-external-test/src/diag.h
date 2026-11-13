#pragma once

#include "config.h"
#include "driver_client.h"
#include "memory.h"
#include "process.h"

#include <string>

namespace fnx {

struct DiagOptions {
    bool self_test = false;
    bool diagnose = false;
    std::wstring process_name;
    DWORD pid = 0;
    std::wstring config_path;
    std::string driver_device_path; // e.g. \\.\FnxTestDrv (optional)
};

int run_self_test();
int run_diagnostics(const DiagOptions& opts);

} // namespace fnx

