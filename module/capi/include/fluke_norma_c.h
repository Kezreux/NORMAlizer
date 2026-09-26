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

/* Parsed *IDN? response. The fields are NUL-terminated; a longer value from the
   instrument is truncated to fit. */
typedef struct norma_identification {
    char manufacturer[64];
    char model[64];
    char serial_number[64];
    char firmware_version[64];
} norma_identification;

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

/* Closes the connection but keeps the handle valid, so norma_last_error() can
   still be read. The handle must still be released with norma_disconnect(). */
FLUKE_NORMA_C_API norma_status norma_close(norma_instrument* instrument);

/* Writes 1 or 0 to *out_open. */
FLUKE_NORMA_C_API norma_status norma_is_open(norma_instrument* instrument, int* out_open);

/*
 * Puts the instrument into a state this library can parse: clears the status
 * registers, switches the transfer format to ASCii, enables concurrent
 * functions and drains a stale error queue. The measurement configuration is
 * left alone.
 *
 * Worth calling right after norma_connect(): the transfer format survives a
 * disconnect, so an instrument left in a binary format by a previous session
 * would answer every measurement query with a block nobody here can read.
 */
FLUKE_NORMA_C_API norma_status norma_prepare(norma_instrument* instrument);

/* Sends *IDN? and fills in the parsed fields. */
FLUKE_NORMA_C_API norma_status norma_identify(norma_instrument* instrument,
                                             norma_identification* out_identification);

/*
 * Reads the configured measurement functions with their status flags
 * (`DATA:STATus?`). Writes at most `capacity` entries into `values` and
 * `status`; *out_count receives the number of measurements in the response.
 * Status entries are the bit flags documented for DATA:STATus? (1 underrange,
 * 2 overrange, 8 undefined, 16 not available, 128 capacitive power factor).
 */
FLUKE_NORMA_C_API norma_status norma_read_data_with_status(norma_instrument* instrument,
                                                          double* values,
                                                          int32_t* status,
                                                          size_t capacity,
                                                          size_t* out_count);

/*
 * Configures the measurement functions (`SENSe:FUNCtion`). `functions` is an
 * array of `count` NUL-terminated names, e.g. {"VOLT1", "CURR1", "POW1:ACT"}.
 */
FLUKE_NORMA_C_API norma_status norma_set_functions(norma_instrument* instrument,
                                                  const char* const* functions,
                                                  size_t count);

/*
 * Drains the instrument error queue. Returns NORMA_OK when it was empty, and
 * NORMA_ERR_SCPI otherwise — norma_last_error() then describes the first entry
 * and norma_last_scpi_code() gives its numeric code.
 */
FLUKE_NORMA_C_API norma_status norma_check_errors(norma_instrument* instrument);

/* Sets the default I/O timeout in milliseconds (0 selects 5000). */
FLUKE_NORMA_C_API norma_status norma_set_timeout(norma_instrument* instrument,
                                                 uint32_t timeout_ms);

/*
 * Last error message for the handle. Pass NULL to get the error of the most
 * recent failed norma_connect() on this thread. Valid until the next call on
 * the same handle/thread.
 */
FLUKE_NORMA_C_API const char* norma_last_error(const norma_instrument* instrument);

/*
 * SCPI code of the most recent NORMA_ERR_SCPI on this handle (a negative number
 * such as -113), or 0 if the last failure was not an instrument error.
 */
FLUKE_NORMA_C_API int norma_last_scpi_code(const norma_instrument* instrument);

/* Library version, e.g. "0.1.0". */
FLUKE_NORMA_C_API const char* norma_version(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FLUKE_NORMA_C_H */
