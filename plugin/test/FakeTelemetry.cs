// A dictionary-backed telemetry source.
//
// This is what lets CONFORMANCE-ADAPTER run in CI, where SimHub's assemblies do not exist. It is
// also what makes C6 ("a missing or renamed source property degrades exactly that element")
// testable at all: removing a key here is precisely the failure the check describes.

using System;
using System.Collections.Generic;
using System.Globalization;
using CydSimDash.Core;

namespace CydSimDash.Test
{
    public sealed class FakeTelemetry : ITelemetrySource
    {
        private readonly Dictionary<string, object> _values =
            new Dictionary<string, object>(StringComparer.Ordinal);

        public bool IsGameRunning { get; set; }
        public string GameName { get; set; }

        /// <summary>Keys that throw when read, to exercise C7 and the fault boundary.</summary>
        public readonly HashSet<string> Explosive = new HashSet<string>(StringComparer.Ordinal);

        public FakeTelemetry()
        {
            IsGameRunning = true;
            GameName = "IRacing";
        }

        public FakeTelemetry Set(string key, object value) { _values[key] = value; return this; }
        public FakeTelemetry Remove(string key) { _values.Remove(key); return this; }

        /// <summary>A source carrying the values observed on 2026-09-22, so the suite's baseline is
        /// a real car rather than an invented one.</summary>
        public static FakeTelemetry Captured()
        {
            return new FakeTelemetry()
                .Set("Gear", "3")
                .Set("Rpms", 6353.0)
                .Set("MaxRpm", 7500.0)
                .Set("DataCorePlugin.GameRawData.SessionData.DriverInfo.DriverCarSLFirstRPM", 6130.0)
                .Set("DataCorePlugin.GameRawData.SessionData.DriverInfo.DriverCarSLShiftRPM", 6690.0)
                .Set("DataCorePlugin.GameRawData.Telemetry.CarLeftRight", 1);
        }

        private bool Lookup(string key, out object v)
        {
            if (Explosive.Contains(key)) throw new InvalidOperationException("boom: " + key);
            return _values.TryGetValue(key, out v);
        }

        public bool TryGetString(string key, out string value)
        {
            object o;
            value = null;
            if (!Lookup(key, out o) || o == null) return false;
            value = o as string;
            if (value == null) value = Convert.ToString(o, CultureInfo.InvariantCulture);
            return true;
        }

        public bool TryGetDouble(string key, out double value)
        {
            object o;
            value = 0.0;
            if (!Lookup(key, out o) || o == null) return false;
            try { value = Convert.ToDouble(o, CultureInfo.InvariantCulture); return true; }
            catch (Exception) { return false; }
        }

        public bool TryGetInt(string key, out int value)
        {
            object o;
            value = 0;
            if (!Lookup(key, out o) || o == null) return false;
            try { value = Convert.ToInt32(o, CultureInfo.InvariantCulture); return true; }
            catch (Exception) { return false; }
        }
    }
}
