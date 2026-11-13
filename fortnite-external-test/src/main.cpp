#include "diag.h"

#include <fcntl.h>
#include <io.h>
#include <iostream>
#include <string>
#include <vector>
#include <windows.h>
#include <shellapi.h>

using fnx::DiagOptions;

static std::string narrow_ascii(const std::wstring& ws) {
    std::string s;
    s.reserve(ws.size());
    for (wchar_t c : ws) {
        if (c >= 0 && c <= 0x7F) s.push_back(static_cast<char>(c));
        else s.push_back('?');
    }
    return s;
}

static void print_usage() {
    std::wcout << L"fn_external_test\n"
               << L"\n"
               << L"  --self-test\n"
               << L"  --diagnose --process <name.exe> [--config <path>] [--driver <\\\\.\\FnxTestDrv>]\n"
               << L"  --diagnose --pid <id> [--config <path>] [--driver <\\\\.\\FnxTestDrv>]\n";
}

int wmain(int argc, wchar_t** argv) {
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stderr), _O_U16TEXT);

    DiagOptions opts;
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"--self-test") {
            opts.self_test = true;
        } else if (arg == L"--diagnose") {
            opts.diagnose = true;
        } else if (arg == L"--process" && i + 1 < argc) {
            opts.process_name = argv[++i];
        } else if (arg == L"--pid" && i + 1 < argc) {
            opts.pid = static_cast<DWORD>(_wtoi(argv[++i]));
        } else if (arg == L"--config" && i + 1 < argc) {
            opts.config_path = argv[++i];
        } else if (arg == L"--driver" && i + 1 < argc) {
            opts.driver_device_path = narrow_ascii(argv[++i]);
        } else if (arg == L"--help" || arg == L"-h" || arg == L"/?") {
            print_usage();
            return 0;
        } else {
            std::wcerr << L"Unknown or incomplete argument: " << arg << L"\n";
            print_usage();
            return 2;
        }
    }

    if (opts.self_test) {
        return fnx::run_self_test();
    }
    if (opts.diagnose) {
        return fnx::run_diagnostics(opts);
    }

    print_usage();
    return 2;
}

int main() {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) {
        std::wcerr << L"Failed to parse command line.\n";
        return 2;
    }
    int rc = wmain(argc, argv);
    LocalFree(argv);
    return rc;
}
