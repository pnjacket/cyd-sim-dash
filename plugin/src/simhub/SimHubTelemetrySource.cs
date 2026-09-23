// The SimHub side of ITelemetrySource.
//
// This file and CydSimDashPlugin.cs are the ONLY two that reference a SimHub assembly. Everything
// in plugin/src/core builds and tests without them, which is what lets the adapter conformance
// suite run in CI where those DLLs do not exist and never will — they are not ours to redistribute.
//
// Every read is wrapped and returns false rather than throwing. That is not defensive habit: it is
// how COMPONENT-ADAPTER-IRACING's per-element degradation (C6, X4) is made structural. An adapter
// cannot take the frame down by reading one absent property, because the read has no failure path.

using System;
using System.Globalization;
using GameReaderCommon;
using SimHub.Plugins;
using CydSimDash.Core;

namespace CydSimDash.SimHubBridge
{
    public sealed class SimHubTelemetrySource : ITelemetrySource
    {
        private PluginManager _pm;
        private GameData _data;

        /// <summary>Re-point at the current update. Reused rather than reallocated: DataUpdate runs
        /// at 60 Hz and allocating a source per frame would make the plugin a garbage generator
        /// inside someone else's process.</summary>
        public void Bind(PluginManager pm, GameData data)
        {
            _pm = pm;
            _data = data;
        }

        public bool IsGameRunning
        {
            get
            {
                try { return _data != null && _data.GameRunning && _data.NewData != null; }
                catch (Exception) { return false; }
            }
        }

        public string GameName
        {
            get
            {
                try { return _data == null ? null : _data.GameName; }
                catch (Exception) { return null; }
            }
        }

        // Resolution order: the typed telemetry object first, then SimHub's property tree by name.
        //
        // Both are needed. Gear, Rpms and MaxRpm live on the typed object; iRacing's own shift-light
        // RPMs and the CarLeftRight enum exist only in the raw property tree, reachable by path.
        private object Lookup(string key)
        {
            try
            {
                StatusDataBase d = _data == null ? null : _data.NewData;
                if (d != null)
                {
                    switch (key)
                    {
                        case "Gear": return d.Gear;
                        case "Rpms": return d.Rpms;
                        case "MaxRpm": return d.MaxRpm;
                    }
                }
            }
            catch (Exception) { /* fall through to the property tree */ }

            try
            {
                return _pm == null ? null : _pm.GetPropertyValue(key);
            }
            catch (Exception)
            {
                return null;
            }
        }

        public bool TryGetString(string key, out string value)
        {
            value = null;
            object o = Lookup(key);
            if (o == null) return false;
            value = o as string;
            if (value == null)
            {
                try { value = Convert.ToString(o, CultureInfo.InvariantCulture); }
                catch (Exception) { return false; }
            }
            return value != null;
        }

        public bool TryGetDouble(string key, out double value)
        {
            value = 0.0;
            object o = Lookup(key);
            if (o == null) return false;
            try { value = Convert.ToDouble(o, CultureInfo.InvariantCulture); }
            catch (Exception) { return false; }
            return !double.IsNaN(value) && !double.IsInfinity(value);
        }

        public bool TryGetInt(string key, out int value)
        {
            value = 0;
            object o = Lookup(key);
            if (o == null) return false;
            try { value = Convert.ToInt32(o, CultureInfo.InvariantCulture); }
            catch (Exception) { return false; }
            return true;
        }
    }
}
