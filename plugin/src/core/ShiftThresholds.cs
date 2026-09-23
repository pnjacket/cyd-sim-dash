// The shift-threshold fallback chain, owned by Domain & Data and stated once there.
//
// It lives in core rather than in the iRacing adapter because every adapter runs the same chain —
// that is what makes CONFORMANCE-ADAPTER C5 assertable against adapters that do not exist yet.
//
// The chain, in order:
//   1. Both points available as absolute RPM, both strictly positive, strictly ordered, and no
//      greater than the redline -> use them.
//   2. Otherwise, a positive redline -> derive from the profile's fractions (0.88 / 0.97).
//   3. Otherwise -> unavailable. Never invent thresholds.

namespace CydSimDash.Core
{
    /// <summary>Per-title fallback constants (`ENTITY-GAMEPROFILE`). Lives on the PC inside its
    /// adapter and is never transmitted.</summary>
    public sealed class GameProfile
    {
        public readonly double RampStartFraction;
        public readonly double FlashFraction;

        public GameProfile(double rampStartFraction, double flashFraction)
        {
            RampStartFraction = rampStartFraction;
            FlashFraction = flashFraction;
        }

        public static GameProfile Default { get { return new GameProfile(0.88, 0.97); } }
    }

    public struct ShiftPair
    {
        public double? RampStartRpm;
        public double? FlashRpm;

        public static ShiftPair Unavailable
        {
            get { ShiftPair p = new ShiftPair(); p.RampStartRpm = null; p.FlashRpm = null; return p; }
        }

        public bool IsAvailable { get { return RampStartRpm.HasValue && FlashRpm.HasValue; } }
    }

    public static class ShiftThresholds
    {
        /// <param name="nativeRampStart">The sim's own first-light RPM, or null if unreadable.</param>
        /// <param name="nativeFlash">The sim's own shift-now RPM, or null if unreadable.</param>
        /// <param name="redline">Maximum RPM, or null if unreadable.</param>
        public static ShiftPair Resolve(
            double? nativeRampStart, double? nativeFlash, double? redline, GameProfile profile)
        {
            // Rung 1 — the sim's own absolute values.
            //
            // `> 0` is load-bearing rather than pedantic. A car with no shift lights configured is
            // expected to report zeros, and zeros satisfy "ordered" under a non-strict reading;
            // accepting them would put the ramp start at 0 rpm and light the panel permanently,
            // which is worse than the fraction rule and much worse than reporting unavailable.
            if (nativeRampStart.HasValue && nativeFlash.HasValue)
            {
                double a = nativeRampStart.Value;
                double b = nativeFlash.Value;
                bool positive = a > 0.0 && b > 0.0;
                bool ordered = a < b;                                  // strict: never equal (C2)
                bool withinRedline = !redline.HasValue || b <= redline.Value;

                if (positive && ordered && withinRedline)
                {
                    ShiftPair p = new ShiftPair();
                    p.RampStartRpm = a;
                    p.FlashRpm = b;
                    return p;
                }
            }

            // Rung 2 — derive from the redline.
            if (redline.HasValue && redline.Value > 0.0)
            {
                double a = redline.Value * profile.RampStartFraction;
                double b = redline.Value * profile.FlashFraction;
                if (a > 0.0 && a < b)
                {
                    ShiftPair p = new ShiftPair();
                    p.RampStartRpm = a;
                    p.FlashRpm = b;
                    return p;
                }
                // A profile with inverted or equal fractions would breach C2. Falling through to
                // unavailable is correct: a misconfigured profile is not a reason to emit a pair
                // the contract forbids.
            }

            // Rung 3 — unavailable. Never invent thresholds.
            return ShiftPair.Unavailable;
        }
    }
}
