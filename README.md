# J2534

Small Windows C++ wrapper around the J2534 v04.04 API.

## Build

```bat
cmake -S . -B build -A Win32
cmake --build build --config Release
```

Use the architecture required by the vendor DLL (many J2534 drivers are
32-bit). The wrapper does not include or provide a vendor driver.
