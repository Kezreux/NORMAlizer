using System;
using System.ComponentModel;
using System.Threading.Tasks;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using NORMAlizer.Desktop.Services;
using NORMAlizer.Interop;

namespace NORMAlizer.Desktop.ViewModels;

public partial class ConnectionViewModel : ViewModelBase
{
    public ConnectionViewModel()
        : this(new InstrumentSession())
    {
    }

    public ConnectionViewModel(InstrumentSession session)
    {
        Session = session;
        Session.PropertyChanged += OnSessionPropertyChanged;

        NativeLibraryStatus = NormaLibrary.TryGetVersion(out var version)
            ? $"Native library: flukenorma_c {version}"
            : "Native library not found — build norma_c with CMake, or set NORMA_C_LIBRARY to its path.";
    }

    public InstrumentSession Session { get; }

    public string NativeLibraryStatus { get; }

    [ObservableProperty]
    public partial string Host { get; set; } = "";

    /// <summary>0 uses the instrument default (23).</summary>
    [ObservableProperty]
    public partial int Port { get; set; } = 23;

    [ObservableProperty]
    public partial int TimeoutMs { get; set; } = 5000;

    [ObservableProperty]
    public partial string? ErrorMessage { get; set; }

    private bool CanConnect => !Session.IsConnected;

    private bool CanDisconnect => Session.IsConnected;

    [RelayCommand(CanExecute = nameof(CanConnect))]
    private async Task ConnectAsync()
    {
        ErrorMessage = null;

        if (string.IsNullOrWhiteSpace(Host))
        {
            ErrorMessage = "Enter a host name or IP address.";
            return;
        }

        if (Port is < 0 or > ushort.MaxValue)
        {
            ErrorMessage = $"Port must be between 0 and {ushort.MaxValue}.";
            return;
        }

        if (TimeoutMs < 0)
        {
            ErrorMessage = "Timeout must be non-negative.";
            return;
        }

        try
        {
            await Session.ConnectAsync(Host.Trim(), (ushort)Port, (uint)TimeoutMs);
        }
        catch (Exception ex) when (ex is NormaException or DllNotFoundException or InvalidOperationException)
        {
            ErrorMessage = ex.Message;
        }
    }

    [RelayCommand(CanExecute = nameof(CanDisconnect))]
    private async Task DisconnectAsync()
    {
        ErrorMessage = null;
        await Session.DisconnectAsync();
    }

    private void OnSessionPropertyChanged(object? sender, PropertyChangedEventArgs e)
    {
        if (e.PropertyName is nameof(InstrumentSession.IsConnected))
        {
            ConnectCommand.NotifyCanExecuteChanged();
            DisconnectCommand.NotifyCanExecuteChanged();
        }
    }
}
