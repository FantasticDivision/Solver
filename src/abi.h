// Plain C ABI over the FitSolver core, for consumers that can't (or
// don't want to) link a C++ Python extension module -- e.g. calling from
// Python via ctypes/cffi, or from another language entirely.
//
// See example/python_example.py for a ctypes-based caller.
#pragma once

#if defined(_WIN32)
#define FITSOLVER_API __declspec(dllexport)
#else
#define FITSOLVER_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Solves a packing request given as a UTF-8 JSON string (see
// docs/README for the {"items": [...], "boxes": [...]} shape) and
// returns a newly-allocated, NUL-terminated UTF-8 JSON string with the
// response. On malformed input, returns a JSON object of the form
// {"success": false, "error": "<message>"}.
//
// The caller owns the returned pointer and MUST release it with
// fitsolver_free() -- it is allocated with the same allocator this
// library was built with (do not call free()/delete[] on it directly
// across a DLL boundary).
FITSOLVER_API char* fitsolver_solve(const char* request_json);

// Frees a string previously returned by fitsolver_solve().
FITSOLVER_API void fitsolver_free(void* pointer);

#ifdef __cplusplus
}
#endif
