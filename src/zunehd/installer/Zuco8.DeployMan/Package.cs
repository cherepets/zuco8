using System.Collections.Generic;
using System.Threading.Tasks;

namespace Zuco8.DeployMan
{
    public delegate void ProgressCallback(string message, double value);

    public interface IPackage
    {
        Task<bool> TryDeployAsync(ProgressCallback progressCallback, out string? errorMessage);
    }

    public class Package: IPackage
    {
        private readonly IReadOnlyCollection<string> _files;

        public Package(IReadOnlyCollection<string> files)
        {
            _files = files;
        }

        public Task<bool> TryDeployAsync(ProgressCallback progressCallback, out string? errorMessage)
        {
            throw new System.NotImplementedException();
        }
    }
}
