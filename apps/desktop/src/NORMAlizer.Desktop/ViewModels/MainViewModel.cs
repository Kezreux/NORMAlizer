using System.ComponentModel;
using CommunityToolkit.Mvvm.ComponentModel;
using NORMAlizer.Desktop.Services;

namespace NORMAlizer.Desktop.ViewModels;

public partial class MainViewModel : ViewModelBase
{
    public MainViewModel()
    {
        Session = new InstrumentSession();
        Connection = new ConnectionViewModel(Session);
        Measurements = new MeasurementsViewModel(Session);
        Console = new ScpiConsoleViewModel(Session);

        Session.PropertyChanged += OnSessionPropertyChanged;
    }

    public InstrumentSession Session { get; }

    public ConnectionViewModel Connection { get; }

    public MeasurementsViewModel Measurements { get; }

    public ScpiConsoleViewModel Console { get; }

    [ObservableProperty]
    public partial string StatusText { get; set; } = "Not connected";

    private void OnSessionPropertyChanged(object? sender, PropertyChangedEventArgs e)
    {
        StatusText = Session.IsConnected
            ? $"Connected to {Session.HostDescription} — {Session.Identity}"
            : "Not connected";
    }
}
