// OUT-CAPTURE / ENTITY-CAPTURE — recording a frame stream to a file.
//
// A capture is ordered lines of {offsetMs, frame}, where `frame` is a VERBATIM EVT-FRAME payload.
// That last word is the contract: the capture and the replay tool are written in different
// languages, and they agree because both serialise the same message, not because they were tested
// against each other.
//
// INV-CAPTURE-CLEAN holds BY CONSTRUCTION rather than by filtering. This writer can only serialise
// a Frame, and no field of Frame is tagged secret or identifying — there is no SSID here, no host,
// no credential, no device identity. A capture cannot leak them because it has no way to reach
// them. Check D9 proves it by searching a real capture for all four.
//
// Note what is deliberately absent: the device table is NOT consulted. Captures record what the
// adapter produced, not what any particular device received, so a capture taken with no device
// connected is just as valid as one taken mid-race.

using System;
using System.Globalization;
using System.IO;
using System.Text;

namespace CydSimDash.Core
{
    public sealed class CaptureWriter : IDisposable
    {
        private StreamWriter _writer;
        private long _firstStampMs = -1;
        private int _framesWritten;

        public string Path { get; private set; }
        public int FramesWritten { get { return _framesWritten; } }

        public CaptureWriter(string path)
        {
            Path = path;
            string dir = System.IO.Path.GetDirectoryName(path);
            if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);
            _writer = new StreamWriter(path, false, new UTF8Encoding(false));
            _writer.AutoFlush = true;   // a capture that vanishes on a crash is worth nothing
        }

        /// <summary>Append a frame. Offsets are relative to the first frame written, so a capture
        /// replays identically regardless of when in the session recording began.</summary>
        public void Write(Frame frame)
        {
            if (_writer == null || frame == null) return;

            if (_firstStampMs < 0) _firstStampMs = frame.Stamp;
            long offset = frame.Stamp - _firstStampMs;
            if (offset < 0) offset = 0;    // a non-monotonic stamp clamps rather than going backwards

            _writer.Write("{\"offsetMs\":");
            _writer.Write(offset.ToString(CultureInfo.InvariantCulture));
            _writer.Write(",\"frame\":");
            _writer.Write(frame.ToJson());
            _writer.Write("}\n");
            _framesWritten++;
        }

        public void Dispose()
        {
            if (_writer == null) return;
            try { _writer.Flush(); _writer.Dispose(); }
            catch (Exception) { }
            _writer = null;
        }
    }
}
