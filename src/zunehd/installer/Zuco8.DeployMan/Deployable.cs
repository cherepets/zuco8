using System.IO;
using System.Reflection;

namespace Zuco8.DeployMan
{
    public interface IDeployable
    {
        string ContainerPath { get; }
        Stream OpenRead();
    }

    public class EmbeddedResource : IDeployable
    {
        public string ContainerPath { get; }

        private readonly Assembly _assembly;
        private readonly string _resourceName;

        public EmbeddedResource(string containerPath, Assembly assembly, string resourceName) 
        {
            ContainerPath = containerPath;

            _assembly = assembly;
            _resourceName = resourceName;
        }

        public Stream OpenRead()
        {
            throw new System.NotImplementedException();
        }
    }

    public class ExternalFile : IDeployable
    {
        public string ContainerPath { get; }

        private readonly string _filePath;

        public ExternalFile(string containerPath, string filePath)
        {
            ContainerPath = containerPath;

            _filePath = filePath;
        }

        public Stream OpenRead()
        {
            throw new System.NotImplementedException();
        }
    }
}
