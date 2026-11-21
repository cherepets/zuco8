using System.Reflection;

namespace Zuco8.DeployMan
{
    public interface IDeployable
    {
    }

    public class EmbeddedResource : IDeployable
    {
        public Assembly Assembly { get; }
        public string Name { get; }

        public EmbeddedResource(Assembly assembly, string name) 
        {
            Assembly = assembly;
            Name = name;
        }
    }

    public class ExternalFile : IDeployable
    {
        public string Path { get; }

        public ExternalFile(string path)
        {
            Path = path;
        }
    }
}
