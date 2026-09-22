namespace NORMAlizer.Interop;

/// <summary>Status codes returned by the native flukenorma_c library (norma_status).</summary>
public enum NormaStatus
{
    Ok = 0,

    /// <summary>Connect, read or write failed.</summary>
    ConnectionError = 1,

    /// <summary>Operation timed out.</summary>
    Timeout = 2,

    /// <summary>Instrument reported an error via SYST:ERR.</summary>
    ScpiError = 3,

    /// <summary>Response could not be parsed.</summary>
    ProtocolError = 4,

    /// <summary>Bad argument from the caller.</summary>
    InvalidArgument = 5,

    /// <summary>Output truncated; retry with a larger buffer.</summary>
    BufferTooSmall = 6,

    Unknown = 7,
}
