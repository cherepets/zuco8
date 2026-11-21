using System.Collections.Generic;
using System.Threading.Tasks;

namespace Zuco8.DeployMan
{
    public delegate void DeploymentProgressCallback(string message, double? value);

    public interface IPackage
    {
        Task<DeploymentResult> TryDeployAsync(DeploymentProgressCallback progressCallback);
    }

    public class Package: IPackage
    {
        private readonly IReadOnlyCollection<IDeployable> _deployables;

        public Package(IReadOnlyCollection<IDeployable> deployables)
        {
            _deployables = deployables;
        }

        public async Task<DeploymentResult> TryDeployAsync(DeploymentProgressCallback progressCallback)
        {
            OnEventProgress(progressCallback, "Deployment started");
            foreach (var deployable in _deployables)
            {
                for (int i = 0; i < 100; i++)
                {
                    OnFileProgress(progressCallback, deployable.ContainerPath, i / 100);
                    await Task.Delay(10);
                }
            }
            OnEventProgress(progressCallback, "Deployment completed");
            return DeploymentResult.Success;
        }

        private void OnFileProgress(DeploymentProgressCallback progressCallback, string message, double value)
        {
            progressCallback?.Invoke(message, value);
        }

        private void OnEventProgress(DeploymentProgressCallback progressCallback, string message)
        {
            progressCallback?.Invoke(message, null);
        }
    }

    public class DeploymentResult
    {
        public bool HasSucceeded { get; }
        public string? ErrorMessage { get; }

        public static DeploymentResult Success { get; } = new DeploymentResult(true, null);
        public static DeploymentResult Failure(string errorMessage) => new(false, errorMessage);

        private DeploymentResult(bool hasSucceeded, string? errorMessage)
        {
            HasSucceeded = hasSucceeded;
            ErrorMessage = errorMessage;
        }
    }
}
