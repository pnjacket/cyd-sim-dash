// COMPONENT-ADAPTER-IRACING — the iRacing title adapter.
//
// This file, and the single registration line in AdapterRegistry, are the ONLY places in the
// product that mention iRacing or an iRacing property. Check R2 asserts exactly that by searching
// the plugin core for any sim name and finding nothing.
//
// Every property name below was observed against a live session on 2026-09-22; the evidence is in
// captures/probe/FINDINGS.md and the mapping table it feeds. Rows that research got wrong are
// called out where they sit, because the wrong value looks just as plausible as the right one.

using CydSimDash.Core;

namespace CydSimDash.Core.Adapters
{
    public sealed class IRacingAdapter : ITitleAdapter
    {
        // The identity SimHub reports, observed literally. Research had recorded "iRacing"; the
        // real value is "IRacing", capital I and capital R. An adapter keyed on the documented
        // spelling would never have matched, and the failure would have presented as
        // "unsupportedTitle" rather than as a typo.
        private const string GameNameValue = "IRacing";

        // Telemetry, read off SimHub's typed object.
        private const string KeyGear = "Gear";
        private const string KeyRpm = "Rpms";
        private const string KeyMaxRpm = "MaxRpm";

        // iRacing's own session data. These are ABSOLUTE RPM and per car — the finding that makes
        // the shift cue the car's rather than a heuristic.
        private const string KeySlFirst = "DataCorePlugin.GameRawData.SessionData.DriverInfo.DriverCarSLFirstRPM";
        private const string KeySlShift = "DataCorePlugin.GameRawData.SessionData.DriverInfo.DriverCarSLShiftRPM";

        // Proximity. The RAW enum, not SimHub's computed SpotterCarLeft/Right.
        //
        // Decided 2026-09-22. The enum's derivation is total over 0..6, so the both-sides case is
        // defined rather than assumed; SimHub's computed fields may well derive as `== 2` and
        // `== 3`, in which case a car on each side would light NEITHER bar — a silent failure of
        // two capabilities at the moment they matter most. The stuck-constant reliability concern
        // that originally motivated preferring the computed fields did not reproduce: the enum
        // tracked correctly through all 5,219 captured samples.
        private const string KeyCarLeftRight = "DataCorePlugin.GameRawData.Telemetry.CarLeftRight";

        private readonly GameProfile _profile = GameProfile.Default;

        public string TitleId { get { return "iracing"; } }

        public bool Matches(string gameName)
        {
            // Ordinal-ignore-case rather than an exact compare: the exact casing is recorded and
            // asserted, but matching on it alone would make a future SimHub respelling a silent
            // no-match rather than a visible one.
            return gameName != null &&
                   string.Equals(gameName, GameNameValue, System.StringComparison.OrdinalIgnoreCase);
        }

        public Frame BuildFrame(ITelemetrySource source, long stamp)
        {
            Frame f = Frame.Blank(Status.Live, TitleId, stamp);

            // Each element is resolved independently, so one unreadable property degrades exactly
            // that element (C6). There is deliberately no shared early return.

            string gear;
            f.Gear = source.TryGetString(KeyGear, out gear) ? NormaliseGear(gear) : null;

            double rpm;
            f.Rpm = source.TryGetDouble(KeyRpm, out rpm) && rpm >= 0.0 ? (double?)rpm : null;

            double redline;
            double? redlineOrNull = source.TryGetDouble(KeyMaxRpm, out redline) ? (double?)redline : null;

            double slFirst, slShift;
            double? nativeRamp = source.TryGetDouble(KeySlFirst, out slFirst) ? (double?)slFirst : null;
            double? nativeFlash = source.TryGetDouble(KeySlShift, out slShift) ? (double?)slShift : null;

            ShiftPair shift = ShiftThresholds.Resolve(nativeRamp, nativeFlash, redlineOrNull, _profile);
            f.RampStartRpm = shift.RampStartRpm;
            f.FlashRpm = shift.FlashRpm;

            int clr;
            if (source.TryGetInt(KeyCarLeftRight, out clr))
            {
                f.SpotterLeft = LeftFromEnum(clr);
                f.SpotterRight = RightFromEnum(clr);
            }
            else
            {
                f.SpotterLeft = null;      // unavailable, NOT "none" — the distinction is contractual
                f.SpotterRight = null;
            }

            return f;
        }

        // iRacing's CarLeftRight enum. The mapping is total: every value in 0..6 has a defined
        // result, and anything outside the domain degrades to unavailable rather than guessing.
        //
        //   0 off · 1 clear · 2 car left · 3 car right
        //   4 cars BOTH sides · 5 two cars left · 6 two cars right
        //
        // 5 and 6 map to "one" rather than "two" because the bars are on-off and v1 never renders a
        // count. The wire vocabulary keeps "two" so that this is a rendering decision rather than a
        // contract limit.
        //
        // Rows 4..6 rest on iRacing's published SDK rather than on observation — they never occurred
        // in the capture and are impractical to stage deliberately. Check X10 settles them by
        // observation during real racing.
        internal static string LeftFromEnum(int v)
        {
            switch (v)
            {
                case 0: return Spotter.None;   // spotter off: no car reported, which is "none"
                case 1: return Spotter.None;
                case 2: return Spotter.One;
                case 3: return Spotter.None;
                case 4: return Spotter.One;
                case 5: return Spotter.One;
                case 6: return Spotter.None;
                default: return null;          // outside the documented domain
            }
        }

        internal static string RightFromEnum(int v)
        {
            switch (v)
            {
                case 0: return Spotter.None;
                case 1: return Spotter.None;
                case 2: return Spotter.None;
                case 3: return Spotter.One;
                case 4: return Spotter.One;
                case 5: return Spotter.None;
                case 6: return Spotter.One;
                default: return null;
            }
        }

        // SimHub reports gear as a string already, in exactly the domain the frame specifies —
        // "R", "N", "1".."9". Observed 2026-09-22 as 1..6, N, R. Anything else becomes unavailable
        // rather than being passed through, so the device never has to defend against the wire.
        private static string NormaliseGear(string g)
        {
            if (string.IsNullOrEmpty(g)) return null;
            string t = g.Trim();
            if (t.Length != 1) return null;
            char c = t[0];
            if (c == 'R' || c == 'r') return "R";
            if (c == 'N' || c == 'n') return "N";
            if (c >= '1' && c <= '9') return t;
            if (c == '0') return "N";       // some titles report neutral as 0
            return null;
        }
    }
}
