/*
 * C ABI for the Fluke NORMA 4000/5000 TCP/SCPI wrapper.
 *
 * This flat, stable interface makes the wrapper usable from any language with
 * a C FFI: C#/.NET (P/Invoke), LabVIEW (Call Library Function Node), Rust,
 * Java (JNA/Panama), Go (cgo), Delphi, MATLAB (loadlibrary), ...
 *
 * All functions return norma_status; NORMA_OK (0) means success. On failure,
 * norma_last_error() returns a human-readable message.
 */
#ifndef FLUKE_NORMA_C_H
#define FLUKE_NORMA_C_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && !defined(FLUKE_NORMA_C_STATIC)
#  if defined(FLUKE_NORMA_C_BUILD)
#    define FLUKE_NORMA_C_API __declspec(dllexport)
#  else
#    define FLUKE_NORMA_C_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__)
#  define FLUKE_NORMA_C_API __attribute__((visibility("default")))
#else
#  define FLUKE_NORMA_C_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handle to a connected instrument. */
typedef struct norma_instrument norma_instrument;

typedef enum norma_status {
    NORMA_OK = 0,
    NORMA_ERR_CONNECTION = 1,       /* connect/read/write failed        */
    NORMA_ERR_TIMEOUT = 2,          /* operation timed out              */
    NORMA_ERR_SCPI = 3,             /* instrument reported SYST:ERR     */
    NORMA_ERR_PROTOCOL = 4,         /* response could not be parsed     */
    NORMA_ERR_INVALID_ARGUMENT = 5, /* bad argument from the caller     */
    NORMA_ERR_BUFFER_TOO_SMALL = 6, /* output truncated                 */
    NORMA_ERR_UNKNOWN = 7
} norma_status;

/*
 * Connects over TCP. `port` 0 selects the default (23); `timeout_ms` 0 selects
 * the default (5000 ms). On success, *out_instrument owns the connection and
 * must be released with norma_disconnect().
 */
FLUKE_NORMA_C_API norma_status norma_connect(const char* host,
                                             uint16_t port,
                                             uint32_t timeout_ms,
                                             norma_instrument** out_instrument);

/* Closes the connection and frees the handle. NULL is allowed. */
FLUKE_NORMA_C_API void norma_disconnect(norma_instrument* instrument);

/* Sends one SCPI setting command (no response), e.g. "*RST". */
FLUKE_NORMA_C_API norma_status norma_write(norma_instrument* instrument,
                                           const char* scpi_command);

/*
 * Sends one SCPI query (e.g. "*IDN?") and copies the NUL-terminated response
 * line into `buffer`. Returns NORMA_ERR_BUFFER_TOO_SMALL if truncated.
 */
FLUKE_NORMA_C_API norma_status norma_query(norma_instrument* instrument,
                                           const char* scpi_query,
                                           char* buffer,
                                           size_t buffer_size);

/*
 * Reads the configured measurement functions (`DATA?`) as doubles.
 * Writes at most `capacity` values into `values`; *out_count receives the
 * number of values in the response.
 */
FLUKE_NORMA_C_API norma_status norma_read_data(norma_instrument* instrument,
                                               double* values,
                                               size_t capacity,
                                               size_t* out_count);

/*
 * Last error message for the handle. Pass NULL to get the error of the most
 * recent failed norma_connect() on this thread. Valid until the next call on
 * the same handle/thread.
 */
FLUKE_NORMA_C_API const char* norma_last_error(const norma_instrument* instrument);

/* Library version, e.g. "0.1.0". */
FLUKE_NORMA_C_API const char* norma_version(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FLUKE_NORMA_C_H */
