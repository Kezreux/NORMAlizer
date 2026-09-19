using CommunityToolkit.Mvvm.ComponentModel;

namespace NORMAlizer.Desktop.ViewModels;

public partial class MainViewModel : ViewModelBase
{
    [ObservableProperty]
    public partial string StatusText { get; set; } = "Not connected";
}
