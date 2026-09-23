// COMPONENT-PLUGIN-CORE's decision logic, minus SimHub.
//
// Split out from the SimHub-facing plugin class so the fault boundary and the status ladder are
// testable without SimHub present. The class that SimHub actually loads is a thin shell over this.
//
// Contains NO sim-specific code: no sim name, no sim property. That is check R2, and keeping the
// registry in its own file is what makes it true here.

using System;

namespace CydSimDash.Core
{
    /// <summary>Where a fault is reported. Kept as an interface so the suite can assert that an
    /// adapter throwing produces exactly ONE log entry rather than a stream (check X5).</summary>
    public interface IFaultSink
    {
        void AdapterFaulted(string titleId, Exception ex);
    }

    public sealed class FrameBuilder
    {
        private readonly AdapterRegistry _registry;
        private readonly IFaultSink _faults;

        // The fault boundary latches per title, so a persistently throwing adapter produces one log
        // entry rather than one per update at 60 Hz. Flooding SimHub's log is the failure the
        // boundary exists to prevent, so it must not be the boundary's own failure mode.
        private string _faultedTitle;

        public FrameBuilder(AdapterRegistry registry, IFaultSink faults)
        {
            _registry = registry;
            _faults = faults;
        }

        /// <summary>The device stores titleId in a fixed 32-byte field, so an over-long name would
        /// be truncated there anyway. Bounding it here means the wire carries what the device will
        /// actually show, rather than something the device silently shortens — and it keeps an
        /// arbitrarily long host string off the wire.</summary>
        private const int MaxTitleIdChars = 31;

        private static string Clamp(string s)
        {
            if (string.IsNullOrEmpty(s)) return null;
            return s.Length <= MaxTitleIdChars ? s : s.Substring(0, MaxTitleIdChars);
        }

        public Frame Build(ITelemetrySource source, long stamp)
        {
            if (source == null || !source.IsGameRunning)
            {
                _faultedTitle = null;
                return Frame.Blank(Status.NoSim, null, stamp);
            }

            string gameName = source.GameName;
            ITitleAdapter adapter = _registry.Select(gameName);
            if (adapter == null)
            {
                _faultedTitle = null;
                // The running title's own name, NOT null. INV-STATUS-CONSISTENT exempts titleId
                // from the unavailable-when-not-live rule precisely here: naming the title is the
                // entire point of this status, and ERR-UNSUPPORTED-TITLE gives the device a screen
                // to name it on. A null would leave that screen with nothing to say.
                //
                // So titleId carries two different things by design, as ENTITY-FRAME describes:
                // the declaring adapter's identity when one matched, and the host's raw title name
                // when none did — because we have no identity of our own for a title we do not
                // support.
                return Frame.Blank(Status.UnsupportedTitle, Clamp(gameName), stamp);
            }

            try
            {
                Frame f = adapter.BuildFrame(source, stamp);
                if (f == null) throw new InvalidOperationException("adapter returned no frame");
                _faultedTitle = null;
                return f;
            }
            catch (Exception ex)
            {
                // The boundary. An adapter is third-party-shaped code from the core's point of
                // view, and it must not be able to take SimHub down or stop the frame stream.
                if (_faultedTitle != adapter.TitleId)
                {
                    _faultedTitle = adapter.TitleId;
                    if (_faults != null) _faults.AdapterFaulted(adapter.TitleId, ex);
                }
                return Frame.Blank(Status.AdapterFault, adapter.TitleId, stamp);
            }
        }
    }
}
