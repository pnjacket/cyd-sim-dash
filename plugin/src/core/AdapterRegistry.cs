// The one place adapters are registered.
//
// This is the composition root, and it is deliberately a separate file from the plugin core: it is
// the single point that names a title, so check R2 can search COMPONENT-PLUGIN-CORE for sim names
// and find nothing. Adding a v2 title is one line here plus one new adapter — no change to the
// core, the publisher, the wire contract, or the firmware.

using System.Collections.Generic;
using CydSimDash.Core.Adapters;

namespace CydSimDash.Core
{
    public sealed class AdapterRegistry
    {
        private readonly List<ITitleAdapter> _adapters;

        public AdapterRegistry() : this(Default()) { }

        public AdapterRegistry(IEnumerable<ITitleAdapter> adapters)
        {
            _adapters = new List<ITitleAdapter>(adapters);
        }

        /// <summary>The shipped set. v1 is iRacing-only by decision; AC/ACC and ETS2 are v2.</summary>
        public static IEnumerable<ITitleAdapter> Default()
        {
            return new ITitleAdapter[]
            {
                new IRacingAdapter(),
            };
        }

        /// <summary>First adapter claiming the running title, or null for a title nothing handles.
        /// First-match-wins, which makes C1 ("selects it for that title and no other") assertable:
        /// two adapters claiming one title is a registration defect the suite catches.</summary>
        public ITitleAdapter Select(string gameName)
        {
            if (string.IsNullOrEmpty(gameName)) return null;
            for (int i = 0; i < _adapters.Count; i++)
            {
                if (_adapters[i].Matches(gameName)) return _adapters[i];
            }
            return null;
        }

        public IList<ITitleAdapter> All { get { return _adapters; } }
    }
}
