using System;
using System.Threading;
using System.Threading.Tasks;
using CommunityToolkit.Mvvm.ComponentModel;
using NORMAlizer.Interop;

namespace NORMAlizer.Desktop.Services;

/// <summary>
/// Owns the single instrument connection shared by all views. Serializes all
/// native calls (the instrument speaks one SCPI exchange at a time) and runs
/// them on the thread pool so the UI stays responsive.
/// </summary>
public sealed partial class InstrumentSession : ObservableObject, IDisposable
{
    private readonly SemaphoreSlim _gate = new(1, 1);
    private NormaInstrument? _instrument;

    [ObservableProperty]
    public partial bool IsConnected { get; private set; }

    /// <summary>*IDN? response of the connected instrument.</summary>
    [ObservableProperty]
    public partial string? Identity { get; private set; }

    /// <summary>"host" or "host:port" of the active connection.</summary>
    [ObservableProperty]
    public partial string? HostDescription { get; private set; }

    public async Task ConnectAsync(string host, ushort port = 0, uint timeoutMs = 0)
    {
        await _gate.WaitAsync();
        try
        {
            if (_instrument is not null)
            {
                throw new InvalidOperationException("Already connected.");
            }

            var (instrument, identity) = await Task.Run(() =>
            {
                var candidate = NormaInstrument.Connect(host, port, timeoutMs);
                try
                {
                    return (candidate, candidate.Query("*IDN?"));
                }
                catch
                {
                    candidate.Dispose();
                    throw;
                }
            });

            _instrument = instrument;
            Identity = identity;
            HostDescription = port == 0 ? host : $"{host}:{port}";
            IsConnected = true;
        }
        finally
        {
            _gate.Release();
        }
    }

    public async Task DisconnectAsync()
    {
        await _gate.WaitAsync();
        try
        {
            if (_instrument is null)
            {
                return;
            }

            var instrument = _instrument;
            _instrument = null;
            await Task.Run(instrument.Dispose);

            IsConnected = false;
            Identity = null;
            HostDescription = null;
        }
        finally
        {
            _gate.Release();
        }
    }

    public Task WriteAsync(string scpiCommand) =>
        InvokeAsync(instrument =>
        {
            instrument.Write(scpiCommand);
            return true;
        });

    public Task<string> QueryAsync(string scpiQuery) =>
        InvokeAsync(instrument => instrument.Query(scpiQuery));

    public Task<double[]> ReadDataAsync() =>
        InvokeAsync(instrument => instrument.ReadData());

    public void Dispose()
    {
        _instrument?.Dispose();
        _instrument = null;
        _gate.Dispose();
    }

    private async Task<T> InvokeAsync<T>(Func<NormaInstrument, T> action)
    {
        await _gate.WaitAsync();
        try
        {
            var instrument = _instrument
                ?? throw new InvalidOperationException("Not connected.");
            return await Task.Run(() => action(instrument));
        }
        finally
        {
            _gate.Release();
        }
    }
}
