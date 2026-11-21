using System.Collections.Generic;
using System.Threading.Tasks;

namespace Zuco8.DeployMan
{
    public delegate void DeploymentProgressCallback(string message, double value);

    public interface IPackage
    {
        Task<DeploymentResult> TryDeployAsync(DeploymentProgressCallback progressCallback);
    }

    public class Package: IPackage
    {
        private readonly IReadOnlyCollection<IDeployable> _files;

        public Package(IReadOnlyCollection<IDeployable> files)
        {
            _files = files;
        }

        public Task<DeploymentResult> TryDeployAsync(DeploymentProgressCallback progressCallback)
        {
            throw new System.NotImplementedException();
        }
    }

    public class DeploymentResult
    {
        public bool HasSucceeded { get; }
        public string? ErrorMessage { get; }

        public DeploymentResult(bool hasSucceeded, string? errorMessage)
        {
            HasSucceeded = hasSucceeded;
            ErrorMessage = errorMessage;
        }
    }
}
