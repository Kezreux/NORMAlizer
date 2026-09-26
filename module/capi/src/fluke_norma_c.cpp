#include "fluke_norma_c.h"

#include <chrono>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <fluke/norma/norma.hpp>

using fluke::norma::ConnectionError;
using fluke::norma::NormaInstrument;
using fluke::norma::ProtocolError;
using fluke::norma::ScpiError;
using fluke::norma::TimeoutError;

struct norma_instrument {
    NormaInstrument instrument;
    std::string last_error;
    int last_scpi_code = 0;
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
        handle->last_scpi_code = e.code();
        return NORMA_ERR_SCPI;
    } catch (const ProtocolError& e) {
        handle->last_error = e.what();
        return NORMA_ERR_PROTOCOL;
    } catch (const ConnectionError& e) {
        handle->last_error = e.what();
        return NORMA_ERR_CONNECTION;
    } catch (const std::invalid_argument& e) {
        // The library validates arguments before putting them on the wire
        // (phase and channel numbers, the aperture range, setup slots, a command
        // carrying a second line, ...). That is a caller mistake, not an
        // instrument or link failure.
        handle->last_error = e.what();
        return NORMA_ERR_INVALID_ARGUMENT;
    } catch (const std::exception& e) {
        handle->last_error = e.what();
        return NORMA_ERR_UNKNOWN;
    } catch (...) {
        handle->last_error = "unknown error";
        return NORMA_ERR_UNKNOWN;
    }
}

/// Copies `text` into a fixed-size C buffer, truncating if needed and always
/// terminating. The identification fields are short by construction, so
/// truncation would mean a firmware that does not follow the manual.
template <std::size_t N>
void copy_field(char (&buffer)[N], const std::string& text) {
    const std::size_t copied = text.copy(buffer, N - 1);
    buffer[copied] = '\0';
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

norma_status norma_close(norma_instrument* instrument) {
    return guarded(instrument, [&] {
        instrument->instrument.close();
        return NORMA_OK;
    });
}

norma_status norma_is_open(norma_instrument* instrument, int* out_open) {
    if (out_open == nullptr) {
        return NORMA_ERR_INVALID_ARGUMENT;
    }
    *out_open = 0;
    return guarded(instrument, [&] {
        *out_open = instrument->instrument.is_open() ? 1 : 0;
        return NORMA_OK;
    });
}

norma_status norma_prepare(norma_instrument* instrument) {
    return guarded(instrument, [&] {
        instrument->instrument.prepare();
        return NORMA_OK;
    });
}

norma_status norma_identify(norma_instrument* instrument,
                            norma_identification* out_identification) {
    if (out_identification == nullptr) {
        return NORMA_ERR_INVALID_ARGUMENT;
    }
    *out_identification = norma_identification{};
    return guarded(instrument, [&] {
        const auto id = instrument->instrument.identify();
        copy_field(out_identification->manufacturer, id.manufacturer);
        copy_field(out_identification->model, id.model);
        copy_field(out_identification->serial_number, id.serial_number);
        copy_field(out_identification->firmware_version, id.firmware_version);
        return NORMA_OK;
    });
}

norma_status norma_set_functions(norma_instrument* instrument,
                                 const char* const* functions, size_t count) {
    if (functions == nullptr || count == 0) {
        return NORMA_ERR_INVALID_ARGUMENT;
    }
    for (size_t i = 0; i < count; ++i) {
        if (functions[i] == nullptr) {
            return NORMA_ERR_INVALID_ARGUMENT;
        }
    }
    return guarded(instrument, [&] {
        std::vector<std::string> names;
        names.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            names.emplace_back(functions[i]);
        }
        instrument->instrument.set_functions(names);
        return NORMA_OK;
    });
}

norma_status norma_check_errors(norma_instrument* instrument) {
    return guarded(instrument, [&] {
        instrument->instrument.check_errors();
        return NORMA_OK;
    });
}

norma_status norma_set_timeout(norma_instrument* instrument, uint32_t timeout_ms) {
    return guarded(instrument, [&] {
        instrument->instrument.set_timeout(
            std::chrono::milliseconds(timeout_ms != 0 ? timeout_ms : 5000));
        return NORMA_OK;
    });
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

norma_status norma_read_data_with_status(norma_instrument* instrument, double* values,
                                        int32_t* status, size_t capacity,
                                        size_t* out_count) {
    if (values == nullptr || status == nullptr || out_count == nullptr) {
        return NORMA_ERR_INVALID_ARGUMENT;
    }
    *out_count = 0;
    return guarded(instrument, [&] {
        const auto reading = instrument->instrument.data_with_status();
        // The library guarantees one status per value, so either bound works as
        // the count; take the smaller in case a future change breaks that.
        const size_t available = reading.values.size() < reading.status.size()
                                     ? reading.values.size()
                                     : reading.status.size();
        *out_count = available;
        const size_t n = available < capacity ? available : capacity;
        for (size_t i = 0; i < n; ++i) {
            values[i] = reading.values[i];
            status[i] = static_cast<int32_t>(reading.status[i]);
        }
        if (available > capacity) {
            instrument->last_error = "value buffer too small (" +
                                     std::to_string(available) + " measurements)";
            return NORMA_ERR_BUFFER_TOO_SMALL;
        }
        return NORMA_OK;
    });
}

const char* norma_last_error(const norma_instrument* instrument) {
    return instrument != nullptr ? instrument->last_error.c_str()
                                 : g_connect_error.c_str();
}

int norma_last_scpi_code(const norma_instrument* instrument) {
    return instrument != nullptr ? instrument->last_scpi_code : 0;
}

const char* norma_version(void) {
    return fluke::norma::kVersionString;
}

} // extern "C"
