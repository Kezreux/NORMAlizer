using System.Runtime.InteropServices;

namespace NORMAlizer.Interop;

/// <summary>Process-wide facts about the native flukenorma_c library.</summary>
public static class NormaLibrary
{
    /// <summary>Version string of the native library, e.g. "0.1.0".</summary>
    /// <exception cref="DllNotFoundException">The native library could not be loaded.</exception>
    public static string Version =>
        Marshal.PtrToStringUTF8(NativeMethods.norma_version()) ?? "unknown";

    /// <summary>
    /// Attempts to load the native library and read its version. Returns false when
    /// flukenorma_c is not on the probing path (build it with CMake, or point the
    /// NORMA_C_LIBRARY environment variable at it).
    /// </summary>
    public static bool TryGetVersion(out string version)
    {
        try
        {
            version = Version;
            return true;
        }
        catch (DllNotFoundException)
        {
            version = string.Empty;
            return false;
        }
    }
}
