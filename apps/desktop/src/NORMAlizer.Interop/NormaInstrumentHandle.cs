using System.Runtime.InteropServices;

namespace NORMAlizer.Interop;

/// <summary>Owns a norma_instrument* and releases it via norma_disconnect().</summary>
internal sealed class NormaInstrumentHandle : SafeHandle
{
    public NormaInstrumentHandle()
        : base(IntPtr.Zero, ownsHandle: true)
    {
    }

    public override bool IsInvalid => handle == IntPtr.Zero;

    protected override bool ReleaseHandle()
    {
        NativeMethods.norma_disconnect(handle);
        return true;
    }
}
