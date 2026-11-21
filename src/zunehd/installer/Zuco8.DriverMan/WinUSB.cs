using System;
using System.Threading.Tasks;

namespace Zuco8.DriverMan
{
    public interface IWinUsb
    {
        Task<bool> IsInstalledAsync();
        Task<DriverInstallationResult> TryInstallAsync();
    }

    public class WinUSB : IWinUsb
    {
        public Task<bool> IsInstalledAsync()
        {
            throw new NotImplementedException();
        }

        public Task<DriverInstallationResult> TryInstallAsync()
        {
            throw new NotImplementedException();
        }
    }

    public class DriverInstallationResult
    {
        public bool HasSucceeded { get; }
        public string? ErrorMessage { get; }

        public DriverInstallationResult(bool hasSucceeded, string? errorMessage)
        {
            HasSucceeded = hasSucceeded;
            ErrorMessage = errorMessage;
        }
    }
}
