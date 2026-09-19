using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Avalonia.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using NORMAlizer.Desktop.Services;
using NORMAlizer.Interop;

namespace NORMAlizer.Desktop.ViewModels;

/// <summary>One row in the measurement table: a configured function and its latest value.</summary>
public sealed record MeasurementRow(int Index, string Function, double Value);

public partial class MeasurementsViewModel : ViewModelBase
{
    private readonly DispatcherTimer _pollTimer;
    private bool _readInProgress;

    public MeasurementsViewModel()
        : this(new InstrumentSession())
    {
    }

    public MeasurementsViewModel(InstrumentSession session)
    {
        Session = session;
        _pollTimer = new DispatcherTimer
        {
            Interval = TimeSpan.FromSeconds(1),
        };
        _pollTimer.Tick += async (_, _) => await PollTickAsync();
    }

    public InstrumentSession Session { get; }

    [ObservableProperty]
    public partial IReadOnlyList<MeasurementRow> Rows { get; set; } = [];

    [ObservableProperty]
    public partial string? ErrorMessage { get; set; }

    [ObservableProperty]
    public partial string? LastReadAt { get; set; }

    public bool IsPolling
    {
        get;
        set
        {
            if (SetProperty(ref field, value))
            {
                if (value)
                {
                    _pollTimer.Start();
                }
                else
                {
                    _pollTimer.Stop();
                }
            }
        }
    }

    [RelayCommand]
    private Task ReadOnceAsync() => ReadAsync();

    private async Task PollTickAsync()
    {
        if (_readInProgress)
        {
            return; // previous read still running; skip this tick
        }

        var ok = await ReadAsync();
        if (!ok)
        {
            IsPolling = false;
        }
    }

    private async Task<bool> ReadAsync()
    {
        _readInProgress = true;
        try
        {
            ErrorMessage = null;

            // SENSe:FUNCtion? returns the configured functions as "f1","f2",...
            // DATA? returns one averaged value per configured function.
            var functionsResponse = await Session.QueryAsync("SENSe:FUNCtion?");
            var names = ParseFunctionNames(functionsResponse);
            var values = await Session.ReadDataAsync();

            Rows = values
                .Select((value, i) => new MeasurementRow(
                    i + 1,
                    i < names.Count ? names[i] : $"value {i + 1}",
                    value))
                .ToArray();
            LastReadAt = $"Last read: {DateTime.Now:HH:mm:ss}";
            return true;
        }
        catch (Exception ex) when (ex is NormaException or InvalidOperationException)
        {
            ErrorMessage = ex.Message;
            return false;
        }
        finally
        {
            _readInProgress = false;
        }
    }

    private static List<string> ParseFunctionNames(string response) =>
        response
            .Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries)
            .Select(name => name.Trim('"'))
            .Where(name => name.Length > 0)
            .ToList();
}
