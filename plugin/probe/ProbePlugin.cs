// cyd-sim-dash — telemetry probe
//
// A READ-ONLY SimHub plugin whose only job is to answer the questions the doc set records as
// unobserved. It closes Integrations' check X1: "every row of the mapping table is observed once
// against a live session".
//
// What it does:
//   - dumps SimHub's entire property inventory once, when a game first appears
//   - samples the properties this product maps, ~10 times a second, to a line-delimited file
//   - tracks the distinct values of the spotter fields, which is the one row research could not
//     settle: whether they count cars or merely flag presence
//
// What it does NOT do: send anything anywhere, change any SimHub setting, or write outside its own
// output folder. Every entry point is wrapped, because a plugin that destabilises SimHub is a far
// worse failure than one that produces no data — the fault boundary is absolute.
//
// Built with the .NET Framework compiler, which is C# 5: no interpolated strings, no null
// conditionals, no expression-bodied members.

using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;
using GameReaderCommon;
using SimHub.Plugins;

namespace CydSimDash
{
    [PluginName("CYD Sim Dash Probe")]
    [PluginAuthor("cyd-sim-dash")]
    [PluginDescription("Read-only telemetry probe. Dumps SimHub's property inventory and samples the properties cyd-sim-dash maps, to Documents\\cyd-probe. Sends nothing and changes nothing.")]
    public class ProbePlugin : IPlugin, IDataPlugin
    {
        public PluginManager PluginManager { get; set; }

        private string _outputDir;
        private StreamWriter _samples;
        private bool _earlyInventoryWritten;
        private bool _lateInventoryWritten;
        private DateTime _lastSample = DateTime.MinValue;
        private int _sampleCount;

        // The rows research could not settle. Distinct values are accumulated so a five-minute
        // session answers "does 2 mean two cars" definitively rather than suggestively.
        private readonly SortedSet<string> _spotterLeftSeen = new SortedSet<string>(StringComparer.Ordinal);
        private readonly SortedSet<string> _spotterRightSeen = new SortedSet<string>(StringComparer.Ordinal);
        private readonly SortedSet<string> _carLeftRightSeen = new SortedSet<string>(StringComparer.Ordinal);
        private readonly SortedSet<string> _gearSeen = new SortedSet<string>(StringComparer.Ordinal);
        private readonly SortedSet<string> _gamesSeen = new SortedSet<string>(StringComparer.Ordinal);

        // Raw session values iRacing publishes per car. These are the ones that matter most: if
        // they are present and absolute, the adapter uses the car's own shift points rather than
        // SimHub's computed approximation.
        private static readonly string[] RawProperties = new string[]
        {
            "DataCorePlugin.CurrentGame",
            "DataCorePlugin.GameRawData.SessionData.DriverInfo.DriverCarSLFirstRPM",
            "DataCorePlugin.GameRawData.SessionData.DriverInfo.DriverCarSLShiftRPM",
            "DataCorePlugin.GameRawData.SessionData.DriverInfo.DriverCarSLLastRPM",
            "DataCorePlugin.GameRawData.SessionData.DriverInfo.DriverCarSLBlinkRPM",
            "DataCorePlugin.GameRawData.SessionData.DriverInfo.DriverCarRedLine",
            "DataCorePlugin.GameRawData.SessionData.DriverInfo.DriverCarIdleRPM",
            "DataCorePlugin.GameRawData.Telemetry.CarLeftRight",
        };

        // ------------------------------------------------------------------

        public void Init(PluginManager pluginManager)
        {
            try
            {
                _outputDir = Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "cyd-probe");
                Directory.CreateDirectory(_outputDir);

                _samples = new StreamWriter(
                    Path.Combine(_outputDir, "samples.ndjson"), false, new UTF8Encoding(false));
                _samples.AutoFlush = true;

                Log("started " + DateTime.Now.ToString("s", CultureInfo.InvariantCulture));
                Log("simhub " + SimHubVersion());
            }
            catch (Exception ex)
            {
                Log("init failed: " + ex.Message);
            }
        }

        public void End(PluginManager pluginManager)
        {
            try
            {
                // A session shorter than the late threshold would otherwise end with only the
                // early dump, which is the one known to be incomplete.
                if (_earlyInventoryWritten && !_lateInventoryWritten && PluginManager != null)
                {
                    WriteInventory(PluginManager, null, "properties-inventory.txt");
                    _lateInventoryWritten = true;
                }
                WriteSummary();
                if (_samples != null) { _samples.Flush(); _samples.Dispose(); _samples = null; }
            }
            catch (Exception ex)
            {
                Log("end failed: " + ex.Message);
            }
        }

        public void DataUpdate(PluginManager pluginManager, ref GameData data)
        {
            // Nothing escapes into SimHub. A probe that takes the rig down would be a much worse
            // outcome than a probe that silently records nothing.
            try
            {
                if (data == null || !data.GameRunning || data.NewData == null) return;

                if (!string.IsNullOrEmpty(data.GameName)) _gamesSeen.Add(data.GameName);

                // Twice, deliberately. iRacing's session data is not populated on the first frame
                // a game reports running, so an early-only dump would show the DriverCarSL* values
                // as null and wrongly suggest they are unavailable.
                if (!_earlyInventoryWritten)
                {
                    WriteInventory(pluginManager, data, "properties-inventory-early.txt");
                    _earlyInventoryWritten = true;
                }
                else if (!_lateInventoryWritten && _sampleCount >= 600)   // ~60 s of sampling
                {
                    WriteInventory(pluginManager, data, "properties-inventory.txt");
                    _lateInventoryWritten = true;
                }

                // Decimated to ~10 Hz: enough to see every threshold crossing and every spotter
                // transition, small enough that a five-minute session is a readable file.
                DateTime now = DateTime.UtcNow;
                if ((now - _lastSample).TotalMilliseconds < 100) return;
                _lastSample = now;

                WriteSample(pluginManager, data);
            }
            catch (Exception)
            {
                // Deliberately silent: logging once per update at 60 Hz would flood SimHub's log,
                // which is the failure this catch exists to avoid in the first place.
            }
        }

        // ------------------------------------------------------------------

        private void WriteInventory(PluginManager pm, GameData data, string fileName)
        {
            try
            {
                List<string> names = pm.GetAllPropertiesNames();
                string path = Path.Combine(_outputDir, fileName);

                using (StreamWriter w = new StreamWriter(path, false, new UTF8Encoding(false)))
                {
                    w.WriteLine("# SimHub property inventory");
                    w.WriteLine("# game: " + (data == null ? "(at shutdown)" : Safe(data.GameName)));
                    w.WriteLine("# captured: " + DateTime.Now.ToString("s", CultureInfo.InvariantCulture));
                    w.WriteLine("# count: " + (names == null ? 0 : names.Count));
                    w.WriteLine();
                    if (names != null)
                    {
                        names.Sort(StringComparer.OrdinalIgnoreCase);
                        foreach (string n in names)
                        {
                            w.WriteLine(n + "\t" + Describe(pm, n));
                        }
                    }
                }
                Log("inventory " + fileName + ": " + (names == null ? 0 : names.Count) + " properties");
            }
            catch (Exception ex)
            {
                Log("inventory " + fileName + " failed: " + ex.Message);
            }
        }

        private static string Describe(PluginManager pm, string name)
        {
            try
            {
                object v = pm.GetPropertyValue(name);
                if (v == null) return "null\t";
                return v.GetType().Name + "\t" + Stringify(v);
            }
            catch (Exception)
            {
                return "<unreadable>\t";
            }
        }

        private void WriteSample(PluginManager pm, GameData data)
        {
            StatusDataBase d = data.NewData;
            StringBuilder sb = new StringBuilder(512);

            sb.Append("{");
            Field(sb, "t", DateTime.UtcNow.ToString("HH:mm:ss.fff", CultureInfo.InvariantCulture));
            Field(sb, "game", Safe(data.GameName));

            // The mapped surface, read straight off the typed telemetry object.
            Field(sb, "Gear", Safe(d.Gear));
            Field(sb, "Rpms", d.Rpms);
            Field(sb, "MaxRpm", d.MaxRpm);
            Field(sb, "Redline", d.Redline);
            Field(sb, "CarSettings_RPMShiftLight1", d.CarSettings_RPMShiftLight1);
            Field(sb, "CarSettings_RPMShiftLight2", d.CarSettings_RPMShiftLight2);
            Field(sb, "CarSettings_MaxRPM", d.CarSettings_MaxRPM);
            Field(sb, "CarSettings_RedLineRPM", d.CarSettings_RedLineRPM);
            Field(sb, "CarSettings_CurrentGearRedLineRPM", d.CarSettings_CurrentGearRedLineRPM);
            Field(sb, "CarSettings_RPMRedLineReached", d.CarSettings_RPMRedLineReached);
            Field(sb, "CarSettings_MaxGears", d.CarSettings_MaxGears);
            Field(sb, "SpotterCarLeft", d.SpotterCarLeft);
            Field(sb, "SpotterCarRight", d.SpotterCarRight);
            Field(sb, "SpotterCarLeftDistance", d.SpotterCarLeftDistance);
            Field(sb, "SpotterCarRightDistance", d.SpotterCarRightDistance);
            Field(sb, "SpotterCarLeftAngle", d.SpotterCarLeftAngle);
            Field(sb, "SpotterCarRightAngle", d.SpotterCarRightAngle);

            _gearSeen.Add(Safe(d.Gear));
            _spotterLeftSeen.Add(d.SpotterCarLeft.ToString(CultureInfo.InvariantCulture));
            _spotterRightSeen.Add(d.SpotterCarRight.ToString(CultureInfo.InvariantCulture));

            // The raw iRacing session values, reached by name because they are not on the typed
            // telemetry object.
            for (int i = 0; i < RawProperties.Length; i++)
            {
                string key = RawProperties[i];
                object v = null;
                try { v = pm.GetPropertyValue(key); }
                catch (Exception) { v = null; }

                string shortKey = key.Substring(key.LastIndexOf('.') + 1);
                if (v == null) FieldRaw(sb, shortKey, "null");
                else Field(sb, shortKey, Stringify(v));

                if (shortKey == "CarLeftRight" && v != null)
                {
                    _carLeftRightSeen.Add(Stringify(v));
                }
            }

            // trim the trailing comma
            if (sb[sb.Length - 1] == ',') sb.Length = sb.Length - 1;
            sb.Append("}");

            _samples.WriteLine(sb.ToString());
            _sampleCount++;
        }

        private void WriteSummary()
        {
            string path = Path.Combine(_outputDir, "summary.txt");
            using (StreamWriter w = new StreamWriter(path, false, new UTF8Encoding(false)))
            {
                w.WriteLine("cyd-sim-dash probe summary");
                w.WriteLine("captured: " + DateTime.Now.ToString("s", CultureInfo.InvariantCulture));
                w.WriteLine("samples:  " + _sampleCount);
                w.WriteLine("simhub:   " + SimHubVersion());
                w.WriteLine();
                w.WriteLine("games seen:             " + Join(_gamesSeen));
                w.WriteLine("distinct Gear values:   " + Join(_gearSeen));
                w.WriteLine();
                w.WriteLine("# The rows research could not settle. If SpotterCarLeft only ever reads");
                w.WriteLine("# 0 and 1, it flags presence; if 2 appears with two cars alongside, it counts.");
                w.WriteLine("distinct SpotterCarLeft:  " + Join(_spotterLeftSeen));
                w.WriteLine("distinct SpotterCarRight: " + Join(_spotterRightSeen));
                w.WriteLine("distinct CarLeftRight:    " + Join(_carLeftRightSeen));
            }
        }

        // ------------------------------------------------------------------
        // Minimal JSON emission. A dependency-free hand-rolled writer is appropriate here: the
        // shape is flat, the probe is throwaway, and adding a package to a build that must compile
        // with nothing but csc.exe would cost more than it saves.

        private static void Field(StringBuilder sb, string key, string value)
        {
            sb.Append('"').Append(key).Append("\":\"").Append(Escape(value)).Append("\",");
        }

        private static void Field(StringBuilder sb, string key, double value)
        {
            if (double.IsNaN(value) || double.IsInfinity(value)) { FieldRaw(sb, key, "null"); return; }
            sb.Append('"').Append(key).Append("\":")
              .Append(value.ToString("0.####", CultureInfo.InvariantCulture)).Append(',');
        }

        private static void Field(StringBuilder sb, string key, int value)
        {
            sb.Append('"').Append(key).Append("\":")
              .Append(value.ToString(CultureInfo.InvariantCulture)).Append(',');
        }

        private static void FieldRaw(StringBuilder sb, string key, string rawValue)
        {
            sb.Append('"').Append(key).Append("\":").Append(rawValue).Append(',');
        }

        private static string Stringify(object v)
        {
            if (v == null) return "";
            if (v is double) return ((double)v).ToString("0.####", CultureInfo.InvariantCulture);
            if (v is float) return ((float)v).ToString("0.####", CultureInfo.InvariantCulture);
            if (v is IFormattable) return ((IFormattable)v).ToString(null, CultureInfo.InvariantCulture);
            return v.ToString();
        }

        private static string Escape(string s)
        {
            if (string.IsNullOrEmpty(s)) return "";
            StringBuilder sb = new StringBuilder(s.Length + 8);
            foreach (char c in s)
            {
                if (c == '"' || c == '\\') sb.Append('\\').Append(c);
                else if (c == '\n' || c == '\r' || c == '\t') sb.Append(' ');
                else if (c < ' ') sb.Append(' ');
                else sb.Append(c);
            }
            return sb.ToString();
        }

        // Read from the host process rather than from the assembly: SimHub does not set a
        // meaningful ProductVersion on its DLLs (they all report 1.0.0.0), so the assembly version
        // cannot serve as the tested-version record the doc set owes.
        private static string SimHubVersion()
        {
            try
            {
                System.Diagnostics.ProcessModule m =
                    System.Diagnostics.Process.GetCurrentProcess().MainModule;
                return m.FileVersionInfo.FileVersion + "  (" + Path.GetFileName(m.FileName) + ")";
            }
            catch (Exception)
            {
                return "(unreadable)";
            }
        }

        private static string Safe(string s) { return s == null ? "" : s; }

        private static string Join(SortedSet<string> set)
        {
            if (set.Count == 0) return "(none)";
            StringBuilder sb = new StringBuilder();
            foreach (string s in set)
            {
                if (sb.Length > 0) sb.Append(", ");
                sb.Append(s.Length == 0 ? "\"\"" : s);
            }
            return sb.ToString();
        }

        // The probe keeps its own log rather than referencing SimHub's. SimHub.Logging.Current is a
        // log4net ILog, so using it would mean shipping a fourth assembly reference for the sake of
        // a handful of status lines — and a log beside the data is easier to send back anyway.
        private void Log(string message)
        {
            try
            {
                string dir = _outputDir;
                if (string.IsNullOrEmpty(dir))
                {
                    dir = Path.Combine(
                        Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "cyd-probe");
                    Directory.CreateDirectory(dir);
                }
                File.AppendAllText(Path.Combine(dir, "probe-log.txt"),
                    DateTime.Now.ToString("s", CultureInfo.InvariantCulture) + "  " + message
                    + Environment.NewLine);
            }
            catch (Exception) { }
        }
    }
}
