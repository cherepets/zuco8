using ReactiveUI;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.Linq;
using System.Threading.Tasks;
using Zuco8.DeployMan;
using Zuco8.DriverMan;

namespace Zuco8.Installer.ViewModels {

    public delegate void UIConfirmationRequestCallback(bool result);
    public delegate void UIConfirmationRequest(string title, string message, string optionYes, string optionNo, UIConfirmationRequestCallback callback);

    public class MainWindowViewModel : ReactiveObject
    {
        public ObservableCollection<string> Roms { get; } = new ObservableCollection<string>();
        public ObservableCollection<string> StatusLog { get; } = new ObservableCollection<string>();
        public string ProgressMessage { get; private set; } = string.Empty;
        public string ProgressPercentage { get; private set; } = string.Empty;
        public bool IsInProgress { get; private set; } = false;
        public event UIConfirmationRequest? ConfirmationRequest;


        public async void Install()
        {
            IsInProgress = true;
            StatusLog.Clear();
            try
            {
                var didInstallWinUsb = false;
                var winUsb = new WinUsb();
                if (await winUsb.IsInstalledAsync())
                {
                    Debug.WriteLine("WinUSB is already installed");
                }
                else
                {
                    var shouldInstall = await ConfirmAsync(
                        title: "WinUSB Driver Installation", 
                        message: "Zune HD driver would be replaced with WinUSB.\r\nDo you want to continue?");
                    if (!shouldInstall)
                    {
                        Debug.WriteLine("User declined WinUSB installation");
                        return;
                    }
                    Debug.WriteLine("Installing WinUSB driver");
                    var winUsbResult = await winUsb.TryInstallAsync();
                    if (!winUsbResult.HasSucceeded)
                    {
                        Debug.WriteLine("WinUSB installation failed");
                        return;
                    }
                    Debug.WriteLine("WinUSB installation succeeded");
                    didInstallWinUsb = true;
                }

                var deployables = new List<IDeployable>();
                deployables.AddRange(Roms.Select(filePath => new ExternalFile("fix", filePath)));
                var package = new Package(deployables);
                var deploymentResult = await package.TryDeployAsync((message, value) =>
                {
                    if (value.HasValue)
                    {
                        ProgressMessage = message;
                        ProgressPercentage = $"{(int)(value * 100)}%";
                    }
                    else
                    {
                        StatusLog.Add(message);
                    }
                });

                if (didInstallWinUsb)
                {
                    var shouldUninstall = await ConfirmAsync(
                        title: "WinUSB Driver Installation",
                        message: "Do you want to revert to the previous Zune HD driver?");
                    if (!shouldUninstall)
                    {
                        Debug.WriteLine("User declined WinUSB uninstallation");
                        return;
                    }
                    Debug.WriteLine("Uninstalling WinUSB driver");
                    var winUsbResult = await winUsb.TryUninstallAsync();
                    if (!winUsbResult.HasSucceeded)
                    {
                        Debug.WriteLine("WinUSB uninstallation failed");
                    }
                    else
                    {
                        Debug.WriteLine("WinUSB uninstallation succeeded");
                    }
                }
            }
            finally
            {
                IsInProgress = false;
            }
        }

        private Task<bool> ConfirmAsync(string title, string message)
        {
            var tcs = new TaskCompletionSource<bool>();
            ConfirmationRequest?.Invoke(title, message, "Yes", "No", tcs.SetResult);
            return tcs.Task;
        }
    }
}