using System.Threading.Tasks;

namespace Zuco8.DriverMan
{
    public interface IWinUsb
    {
        Task<bool> IsInstalledAsync();
        Task<DriverInstallationResult> TryInstallAsync();
        Task<DriverInstallationResult> TryUninstallAsync();
    }

    public class WinUsb : IWinUsb
    {
        public async Task<bool> IsInstalledAsync()
        {
            await Task.Delay(1000);
            return false;
        }

        public async Task<DriverInstallationResult> TryInstallAsync()
        {
            await Task.Delay(1000);
            return DriverInstallationResult.Success;
        }

        public async Task<DriverInstallationResult> TryUninstallAsync()
        {
            await Task.Delay(1000);
            return DriverInstallationResult.Success;
        }
    }

    public class DriverInstallationResult
    {
        public bool HasSucceeded { get; }
        public string? ErrorMessage { get; }

        public static DriverInstallationResult Success { get; } = new DriverInstallationResult(true, null);
        public static DriverInstallationResult Failure(string errorMessage) => new(false, errorMessage);

        private DriverInstallationResult(bool hasSucceeded, string? errorMessage)
        {
            HasSucceeded = hasSucceeded;
            ErrorMessage = errorMessage;
        }
    }
}
