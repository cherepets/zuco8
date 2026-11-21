using System;
using System.Threading.Tasks;

namespace Zuco8.DriverMan
{
    public interface IWinUsb
    {
        Task<bool> IsInstalledAsync();
        Task<bool> TryInstallAsync(out string? errorMessage);
    }

    public class WinUSB : IWinUsb
    {
        public Task<bool> IsInstalledAsync()
        {
            throw new NotImplementedException();
        }

        public Task<bool> TryInstallAsync(out string? errorMessage)
        {
            throw new NotImplementedException();
        }
    }
}
