#include "fluke_norma_c.h"

#include <cstring>
#include <string>
#include <utility>

#include <fluke/norma/norma.hpp>

using fluke::norma::ConnectionError;
using fluke::norma::NormaInstrument;
using fluke::norma::ProtocolError;
using fluke::norma::ScpiError;
using fluke::norma::TimeoutError;

struct norma_instrument {
    NormaInstrument instrument;
    std::string last_error;
};

namespace {

thread_local std::string g_connect_error;

template <typename Fn>
norma_status guarded(norma_instrument* handle, Fn&& fn) {
    if (handle == nullptr) {
        return NORMA_ERR_INVALID_ARGUMENT;
    }
    try {
        return fn();
    } catch (const TimeoutError& e) {
        handle->last_error = e.what();
        return NORMA_ERR_TIMEOUT;
    } catch (const ScpiError& e) {
        handle->last_error = e.what();
        return NORMA_ERR_SCPI;
    } catch (const ProtocolError& e) {
        handle->last_error = e.what();
        return NORMA_ERR_PROTOCOL;
    } catch (const ConnectionError& e) {
        handle->last_error = e.what();
        return NORMA_ERR_CONNECTION;
    } catch (const std::exception& e) {
        handle->last_error = e.what();
        return NORMA_ERR_UNKNOWN;
    } catch (...) {
        handle->last_error = "unknown error";
        return NORMA_ERR_UNKNOWN;
    }
}

} // namespace

extern "C" {

norma_status norma_connect(const char* host, uint16_t port, uint32_t timeout_ms,
                           norma_instrument** out_instrument) {
    if (host == nullptr || out_instrument == nullptr) {
        return NORMA_ERR_INVALID_ARGUMENT;
    }
    *out_instrument = nullptr;
    try {
        auto instrument = NormaInstrument::connect(
            host,
            port != 0 ? port : fluke::norma::kDefaultPort,
            std::chrono::milliseconds(timeout_ms != 0 ? timeout_ms : 5000));
        *out_instrument = new norma_instrument{std::move(instrument), {}};
        return NORMA_OK;
    } catch (const TimeoutError& e) {
        g_connect_error = e.what();
        return NORMA_ERR_TIMEOUT;
    } catch (const std::exception& e) {
        g_connect_error = e.what();
        return NORMA_ERR_CONNECTION;
    } catch (...) {
        g_connect_error = "unknown error";
        return NORMA_ERR_CONNECTION;
    }
}

void norma_disconnect(norma_instrument* instrument) {
    delete instrument;
}

norma_status norma_write(norma_instrument* instrument, const char* scpi_command) {
    if (scpi_command == nullptr) {
        return NORMA_ERR_INVALID_ARGUMENT;
    }
    return guarded(instrument, [&] {
        instrument->instrument.write(scpi_command);
        return NORMA_OK;
    });
}

norma_status norma_query(norma_instrument* instrument, const char* scpi_query,
                         char* buffer, size_t buffer_size) {
    if (scpi_query == nullptr || buffer == nullptr || buffer_size == 0) {
        return NORMA_ERR_INVALID_ARGUMENT;
    }
    return guarded(instrument, [&] {
        const std::string response = instrument->instrument.query(scpi_query);
        const size_t copied = response.copy(buffer, buffer_size - 1);
        buffer[copied] = '\0';
        if (copied < response.size()) {
            instrument->last_error = "response truncated (" +
                                     std::to_string(response.size() + 1) +
                                     " bytes needed)";
            return NORMA_ERR_BUFFER_TOO_SMALL;
        }
        return NORMA_OK;
    });
}

norma_status norma_read_data(norma_instrument* instrument, double* values,
                             size_t capacity, size_t* out_count) {
    if (values == nullptr || out_count == nullptr) {
        return NORMA_ERR_INVALID_ARGUMENT;
    }
    *out_count = 0;
    return guarded(instrument, [&] {
        const auto data = instrument->instrument.data();
        *out_count = data.size();
        const size_t n = data.size() < capacity ? data.size() : capacity;
        std::memcpy(values, data.data(), n * sizeof(double));
        if (data.size() > capacity) {
            instrument->last_error = "value buffer too small (" +
                                     std::to_string(data.size()) + " values)";
            return NORMA_ERR_BUFFER_TOO_SMALL;
        }
        return NORMA_OK;
    });
}

const char* norma_last_error(const norma_instrument* instrument) {
    return instrument != nullptr ? instrument->last_error.c_str()
                                 : g_connect_error.c_str();
}

const char* norma_version(void) {
    return fluke::norma::kVersionString;
}

} // extern "C"
