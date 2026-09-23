// The adapter's only window onto the sim.
//
// Every read returns false rather than throwing when a property is missing, renamed, or of an
// unexpected type. That is what makes COMPONENT-ADAPTER-IRACING's per-element degradation (C6, X4)
// a property of the code rather than a promise: an adapter physically cannot take the whole frame
// down by reading one absent property, because the read has no failure path that propagates.
//
// It also decouples the adapters from SimHub entirely. The real implementation wraps GameData and
// PluginManager; the conformance suite supplies a dictionary. Neither the adapters nor the suite
// reference a SimHub assembly, which is why both build in CI where those DLLs do not exist.

namespace CydSimDash.Core
{
    public interface ITelemetrySource
    {
        /// <summary>True when a game is running and producing telemetry.</summary>
        bool IsGameRunning { get; }

        /// <summary>The running title's identity, as the host reports it. Null when none.</summary>
        string GameName { get; }

        bool TryGetString(string key, out string value);
        bool TryGetDouble(string key, out double value);
        bool TryGetInt(string key, out int value);
    }
}
