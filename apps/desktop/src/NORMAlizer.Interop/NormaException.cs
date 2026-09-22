namespace NORMAlizer.Interop;

/// <summary>Raised when a call into the native flukenorma_c library fails.</summary>
public sealed class NormaException : Exception
{
    public NormaException(NormaStatus status, string message)
        : base(message)
    {
        Status = status;
    }

    public NormaStatus Status { get; }
}
