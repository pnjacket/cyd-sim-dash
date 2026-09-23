// The title-adapter interface — the seam that keeps per-sim knowledge out of everything else.
//
// ADR-ADAPTER-MODULES: each title is a source module implementing this interface, registered in one
// place and shipped in a single assembly. Adding a title is a new implementation plus one
// registration line; it is never a change to the plugin core, the publisher, the wire contract, or
// the firmware.
//
// An adapter resolves EVERYTHING sim-specific, including the shift-threshold fallback chain, and
// emits absolute RPM thresholds. That is what allows the firmware to contain no per-title logic —
// and, as the 2026-09-22 capture made concrete, no per-car logic either.

namespace CydSimDash.Core
{
    public interface ITitleAdapter
    {
        /// <summary>The single title identity this adapter declares (`CONFORMANCE-ADAPTER` C1).
        /// This is the value that reaches the wire as `titleId`.</summary>
        string TitleId { get; }

        /// <summary>Whether this adapter handles the named running title.
        ///
        /// Matching is the adapter's own business rather than a string equality the registry
        /// performs, because the host's spelling is the host's to know. iRacing's is `"IRacing"` —
        /// capital I and capital R — which research had recorded as `"iRacing"`; a registry
        /// comparing against the documented spelling would have silently matched nothing.</summary>
        bool Matches(string gameName);

        /// <summary>Resolve a frame from the source. Must not throw for input the title can
        /// legitimately produce (C7); the plugin core's fault boundary is a backstop, not a
        /// substitute for this.</summary>
        Frame BuildFrame(ITelemetrySource source, long stamp);
    }
}
