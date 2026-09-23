// Where faults are reported.
//
// Latching is the whole point. DataUpdate runs at roughly 60 Hz, so a persistent fault reported on
// every update writes 3,600 identical lines a minute into SimHub's log — turning a diagnosable
// problem into an unreadable one, and making this plugin a nuisance inside someone else's
// application. Check X5 asserts "one log entry rather than a stream".
//
// Writes to SimHub's own log if that is reachable and to a file beside the plugin otherwise, so a
// fault is still recoverable when SimHub's logging is not available.

using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using CydSimDash.Core;

namespace CydSimDash.SimHubBridge
{
    public sealed class LogSink : IFaultSink
    {
        private readonly HashSet<string> _seen = new HashSet<string>(StringComparer.Ordinal);
        private readonly string _path;

        public LogSink()
        {
            try
            {
                string dir = Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "cyd-sim-dash");
                Directory.CreateDirectory(dir);
                _path = Path.Combine(dir, "plugin-log.txt");
            }
            catch (Exception)
            {
                _path = null;
            }
        }

        public void AdapterFaulted(string titleId, Exception ex)
        {
            // FrameBuilder already latches per title, so this arrives once per fault episode. The
            // second latch here covers everything else that reports through Once().
            Write("adapter fault [" + titleId + "]: " + Describe(ex));
        }

        /// <summary>Report once per distinct key for the process lifetime.</summary>
        public void Once(string key, Exception ex)
        {
            if (_seen.Contains(key)) return;
            _seen.Add(key);
            Write("fault [" + key + "]: " + Describe(ex) + "   (further occurrences suppressed)");
        }

        public void Info(string message) { Write(message); }

        private static string Describe(Exception ex)
        {
            if (ex == null) return "(none)";
            return ex.GetType().Name + ": " + ex.Message;
        }

        private void Write(string message)
        {
            // Its own file rather than SimHub's log. SimHub.Logging.Current is a log4net ILog, so
            // using it would add a fourth referenced assembly for the sake of a handful of lines —
            // and a log the operator can find and send back is worth more here than one buried in
            // SimHub's own.
            string line = DateTime.Now.ToString("s", CultureInfo.InvariantCulture) + "  " + message;
            try
            {
                if (_path != null) File.AppendAllText(_path, line + Environment.NewLine);
            }
            catch (Exception) { }
        }
    }
}
