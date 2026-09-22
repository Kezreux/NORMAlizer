using System.Reflection;
using System.Runtime.InteropServices;

namespace NORMAlizer.Interop;

/// <summary>
/// Raw P/Invoke surface of flukenorma_c (module/capi/include/fluke_norma_c.h).
/// The native library is resolved by the default probing rules; set the
/// NORMA_C_LIBRARY environment variable to the full path of the DLL/.so to
/// load it from a CMake build tree instead.
/// </summary>
internal static partial class NativeMethods
{
    internal const string LibraryName = "flukenorma_c";

    // Runs before the first P/Invoke in this class can trigger a library load.
    static NativeMethods()
    {
        NativeLibrary.SetDllImportResolver(typeof(NativeMethods).Assembly, Resolve);
    }

    private static IntPtr Resolve(string name, Assembly assembly, DllImportSearchPath? searchPath)
    {
        if (name == LibraryName)
        {
            var overridePath = Environment.GetEnvironmentVariable("NORMA_C_LIBRARY");
            if (!string.IsNullOrWhiteSpace(overridePath)
                && NativeLibrary.TryLoad(overridePath, out var library))
            {
                return library;
            }
        }

        return IntPtr.Zero; // fall back to default resolution
    }

    [LibraryImport(LibraryName, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial NormaStatus norma_connect(
        string host, ushort port, uint timeoutMs, out NormaInstrumentHandle outInstrument);

    [LibraryImport(LibraryName)]
    internal static partial void norma_disconnect(IntPtr instrument);

    [LibraryImport(LibraryName, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial NormaStatus norma_write(
        NormaInstrumentHandle instrument, string scpiCommand);

    [LibraryImport(LibraryName, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial NormaStatus norma_query(
        NormaInstrumentHandle instrument, string scpiQuery, byte[] buffer, nuint bufferSize);

    [LibraryImport(LibraryName)]
    internal static partial NormaStatus norma_read_data(
        NormaInstrumentHandle instrument, [Out] double[] values, nuint capacity, out nuint outCount);

    [LibraryImport(LibraryName)]
    internal static partial IntPtr norma_last_error(NormaInstrumentHandle instrument);

    /// <summary>norma_last_error(NULL): thread-local error of the last failed connect.</summary>
    [LibraryImport(LibraryName, EntryPoint = "norma_last_error")]
    internal static partial IntPtr norma_last_connect_error(IntPtr nullInstrument);

    [LibraryImport(LibraryName)]
    internal static partial IntPtr norma_version();
}
