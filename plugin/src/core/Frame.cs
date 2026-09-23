// The normalised frame — what every adapter produces and the publisher serialises.
//
// This is the wire contract EVT-FRAME expressed in C#. It deliberately contains no SimHub type and
// no sim name, so it compiles and tests on a machine with no SimHub installed — which is what lets
// the adapter conformance suite run in CI.
//
// `null` means UNAVAILABLE and is distinct from a cleared value. Every key is always present on the
// wire; an element the title cannot source is emitted as null rather than omitted or zeroed.

using System;
using System.Globalization;
using System.Text;

namespace CydSimDash.Core
{
    /// <summary>Frame status. String constants rather than an enum: these are wire values, and the
    /// wire contract owns their spelling.</summary>
    public static class Status
    {
        public const string Live = "live";
        public const string NoSim = "noSim";
        public const string UnsupportedTitle = "unsupportedTitle";
        public const string AdapterFault = "adapterFault";
    }

    /// <summary>Per-side proximity. `null` is unavailable; "none" is a car-free side, and the two
    /// are not interchangeable.</summary>
    public static class Spotter
    {
        public const string None = "none";
        public const string One = "one";
        public const string Two = "two";
    }

    public sealed class Frame
    {
        public const int ProtocolMajor = 1;
        public const int ProtocolMinor = 0;

        public string StatusValue;
        public string TitleId;
        public long Stamp;
        public string Gear;
        public double? Rpm;
        public double? RampStartRpm;
        public double? FlashRpm;
        public string SpotterLeft;
        public string SpotterRight;

        /// <summary>A frame carrying no telemetry — every element unavailable. Used for every
        /// status other than `live`, so that a non-live frame never carries stale values.</summary>
        public static Frame Blank(string status, string titleId, long stamp)
        {
            Frame f = new Frame();
            f.StatusValue = status;
            f.TitleId = titleId;
            f.Stamp = stamp;
            f.Gear = null;
            f.Rpm = null;
            f.RampStartRpm = null;
            f.FlashRpm = null;
            f.SpotterLeft = null;
            f.SpotterRight = null;
            return f;
        }

        // Hand-rolled, for the same reason the probe's was: the shape is flat and fixed, and the
        // plugin must build with nothing but the Framework compiler. A JSON dependency here would
        // have to be redistributed alongside the plugin, which POLICY-NO-REDISTRIBUTION-SIMHUB's
        // neighbouring policies make more trouble than eleven fields are worth.
        public string ToJson()
        {
            StringBuilder sb = new StringBuilder(256);
            sb.Append('{');
            Num(sb, "protocolMajor", ProtocolMajor);
            Num(sb, "protocolMinor", ProtocolMinor);
            Str(sb, "status", StatusValue);
            Str(sb, "titleId", TitleId);
            Num(sb, "stamp", Stamp);
            Str(sb, "gear", Gear);
            Dbl(sb, "rpm", Rpm);
            Dbl(sb, "rampStartRpm", RampStartRpm);
            Dbl(sb, "flashRpm", FlashRpm);
            Str(sb, "spotterLeft", SpotterLeft);
            Str(sb, "spotterRight", SpotterRight);
            if (sb[sb.Length - 1] == ',') sb.Length = sb.Length - 1;
            sb.Append('}');
            return sb.ToString();
        }

        private static void Num(StringBuilder sb, string k, long v)
        {
            sb.Append('"').Append(k).Append("\":").Append(v.ToString(CultureInfo.InvariantCulture)).Append(',');
        }

        private static void Str(StringBuilder sb, string k, string v)
        {
            sb.Append('"').Append(k).Append("\":");
            if (v == null) sb.Append("null,");
            else sb.Append('"').Append(Escape(v)).Append("\",");
        }

        private static void Dbl(StringBuilder sb, string k, double? v)
        {
            sb.Append('"').Append(k).Append("\":");
            if (!v.HasValue || double.IsNaN(v.Value) || double.IsInfinity(v.Value)) sb.Append("null,");
            else sb.Append(v.Value.ToString("0.####", CultureInfo.InvariantCulture)).Append(',');
        }

        private static string Escape(string s)
        {
            StringBuilder sb = new StringBuilder(s.Length + 8);
            foreach (char c in s)
            {
                if (c == '"' || c == '\\') sb.Append('\\').Append(c);
                else if (c < ' ') sb.Append(' ');
                else sb.Append(c);
            }
            return sb.ToString();
        }
    }
}
