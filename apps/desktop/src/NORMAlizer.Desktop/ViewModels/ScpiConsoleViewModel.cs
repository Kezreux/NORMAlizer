using System;
using System.Collections.ObjectModel;
using System.Threading.Tasks;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using NORMAlizer.Desktop.Services;
using NORMAlizer.Interop;

namespace NORMAlizer.Desktop.ViewModels;

/// <summary>One line in the console log.</summary>
public sealed record ConsoleEntry(string Prefix, string Text)
{
    public bool IsError => Prefix == "!";
}

public partial class ScpiConsoleViewModel : ViewModelBase
{
    public ScpiConsoleViewModel()
        : this(new InstrumentSession())
    {
    }

    public ScpiConsoleViewModel(InstrumentSession session)
    {
        Session = session;
    }

    public InstrumentSession Session { get; }

    public ObservableCollection<ConsoleEntry> Log { get; } = [];

    [ObservableProperty]
    public partial string CommandText { get; set; } = "";

    /// <summary>Sends the command: queries (ending in '?') log the response, settings are written.</summary>
    [RelayCommand]
    private async Task SendAsync()
    {
        var command = CommandText.Trim();
        if (command.Length == 0)
        {
            return;
        }

        Log.Add(new ConsoleEntry(">", command));
        CommandText = "";

        try
        {
            if (command.EndsWith('?'))
            {
                var response = await Session.QueryAsync(command);
                Log.Add(new ConsoleEntry("<", response));
            }
            else
            {
                await Session.WriteAsync(command);
            }
        }
        catch (Exception ex) when (ex is NormaException or InvalidOperationException)
        {
            Log.Add(new ConsoleEntry("!", ex.Message));
        }
    }

    [RelayCommand]
    private void ClearLog() => Log.Clear();
}
