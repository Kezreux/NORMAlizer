using System.Runtime.InteropServices;
using System.Text;

namespace NORMAlizer.Interop;

/// <summary>
/// Managed wrapper around one flukenorma_c instrument connection.
///
/// Instances are not thread-safe; serialize calls per instance. All methods
/// block on network I/O, so call them off the UI thread.
/// </summary>
public sealed class NormaInstrument : IDisposable
{
    private const int InitialQueryBufferBytes = 1024;
    private const int MaxQueryBufferBytes = 1 << 20;
    private const int InitialDataCapacity = 64;

    private readonly NormaInstrumentHandle _handle;

    private NormaInstrument(NormaInstrumentHandle handle)
    {
        _handle = handle;
    }

    /// <summary>
    /// Connects over TCP. <paramref name="port"/> 0 selects the instrument default (23);
    /// <paramref name="timeoutMs"/> 0 selects the default (5000 ms).
    /// </summary>
    /// <exception cref="NormaException">The connection failed.</exception>
    public static NormaInstrument Connect(string host, ushort port = 0, uint timeoutMs = 0)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(host);

        var status = NativeMethods.norma_connect(host, port, timeoutMs, out var handle);
        if (status != NormaStatus.Ok)
        {
            handle.Dispose();
            var message = Marshal.PtrToStringUTF8(NativeMethods.norma_last_connect_error(IntPtr.Zero));
            throw new NormaException(status, string.IsNullOrEmpty(message)
                ? $"Failed to connect to {host}: {status}"
                : message);
        }

        return new NormaInstrument(handle);
    }

    /// <summary>Sends one SCPI setting command (no response), e.g. "*RST".</summary>
    public void Write(string scpiCommand)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(scpiCommand);
        ThrowIfDisposed();
        ThrowIfError(NativeMethods.norma_write(_handle, scpiCommand));
    }

    /// <summary>Sends one SCPI query (e.g. "*IDN?") and returns the response line.</summary>
    public string Query(string scpiQuery)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(scpiQuery);
        ThrowIfDisposed();

        for (var size = InitialQueryBufferBytes; size <= MaxQueryBufferBytes; size *= 2)
        {
            var buffer = new byte[size];
            var status = NativeMethods.norma_query(_handle, scpiQuery, buffer, (nuint)buffer.Length);
            if (status == NormaStatus.BufferTooSmall)
            {
                continue;
            }

            ThrowIfError(status);
            var length = Array.IndexOf(buffer, (byte)0);
            return Encoding.UTF8.GetString(buffer, 0, length >= 0 ? length : buffer.Length);
        }

        throw new NormaException(
            NormaStatus.BufferTooSmall,
            $"SCPI response exceeded {MaxQueryBufferBytes} bytes.");
    }

    /// <summary>Reads the configured measurement functions (DATA?) as doubles.</summary>
    public double[] ReadData()
    {
        ThrowIfDisposed();

        var values = new double[InitialDataCapacity];
        var status = NativeMethods.norma_read_data(_handle, values, (nuint)values.Length, out var count);
        if (status == NormaStatus.BufferTooSmall && count > (nuint)values.Length)
        {
            // Retries with the reported size; re-queries the instrument.
            values = new double[count];
            status = NativeMethods.norma_read_data(_handle, values, (nuint)values.Length, out count);
        }

        ThrowIfError(status);
        Array.Resize(ref values, checked((int)count));
        return values;
    }

    /// <summary>Closes the connection and frees the native handle.</summary>
    public void Dispose() => _handle.Dispose();

    private void ThrowIfDisposed() =>
        ObjectDisposedException.ThrowIf(_handle.IsClosed || _handle.IsInvalid, this);

    private void ThrowIfError(NormaStatus status)
    {
        if (status == NormaStatus.Ok)
        {
            return;
        }

        var message = Marshal.PtrToStringUTF8(NativeMethods.norma_last_error(_handle));
        throw new NormaException(status, string.IsNullOrEmpty(message)
            ? $"Native call failed: {status}"
            : message);
    }
}
