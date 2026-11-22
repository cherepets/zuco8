using ReactiveUI;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.CompilerServices;

namespace Zuco8.Installer.ViewModels
{
    public abstract class ViewModelBase : ReactiveObject
    {
        private readonly Dictionary<string, object?> _valueStore = new();

        protected T? Get<T>([CallerMemberName] string? property = null)
        {
            Debug.Assert(property != null);

            return _valueStore.TryGetValue(property, out var value) && value is T typed
                ? typed
                : default;
        }

        protected T Set<T>(T value, [CallerMemberName] string? property = null)
        {
            Debug.Assert(property != null);

            _valueStore[property] = value!;
            this.RaisePropertyChanged(property);
            return value;
        }

    }
}
