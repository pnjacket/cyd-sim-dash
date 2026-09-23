// The device table — who is currently listening.
//
// Device-initiated addressing is the whole point of the design: the device holds the PC's address
// and announces itself, so the plugin holds no configuration at all. This table is the entirety of
// that state, and it lives only in memory — nothing here is ever persisted, because a device that
// stops registering has stopped caring and a stale entry is just traffic sent into the void.
//
// EVT-REGISTRATION: devices register every 2 s and are forgotten after 6 s without one. Three
// missed keepalives rather than one, so a brief WiFi stall does not drop a device mid-corner.
//
// Registration is idempotent by construction: the table is keyed on device identity, so a repeat
// refreshes the entry rather than adding one (check I8). A device that changes address — a DHCP
// lease moving, say — updates in place rather than being served twice.

using System;
using System.Collections.Generic;
using System.Net;

namespace CydSimDash.Core
{
    public sealed class DeviceEntry
    {
        public string DeviceId;
        public IPEndPoint EndPoint;
        public string FirmwareVersion;
        public long LastSeenMs;
    }

    public sealed class DeviceTable
    {
        /// <summary>Three missed keepalives at the 2 s registration interval.</summary>
        public const long ForgetAfterMs = 6000;

        private readonly Dictionary<string, DeviceEntry> _devices =
            new Dictionary<string, DeviceEntry>(StringComparer.Ordinal);
        private readonly object _lock = new object();

        /// <summary>Record or refresh a registration. Returns false if the identity is malformed,
        /// in which case nothing is recorded (check I9).</summary>
        public bool Register(string deviceId, IPEndPoint source, string firmwareVersion, long nowMs)
        {
            if (!IsValidDeviceId(deviceId) || source == null) return false;

            lock (_lock)
            {
                DeviceEntry e;
                if (!_devices.TryGetValue(deviceId, out e))
                {
                    e = new DeviceEntry();
                    e.DeviceId = deviceId;
                    _devices[deviceId] = e;
                }
                e.EndPoint = source;                 // the address we ACTUALLY heard from
                e.FirmwareVersion = firmwareVersion;
                e.LastSeenMs = nowMs;
            }
            return true;
        }

        /// <summary>The devices to send this frame to, dropping any that have gone quiet.</summary>
        public List<DeviceEntry> ActiveDevices(long nowMs)
        {
            List<DeviceEntry> active = new List<DeviceEntry>();
            lock (_lock)
            {
                List<string> expired = null;
                foreach (KeyValuePair<string, DeviceEntry> kv in _devices)
                {
                    if (nowMs - kv.Value.LastSeenMs > ForgetAfterMs)
                    {
                        if (expired == null) expired = new List<string>();
                        expired.Add(kv.Key);
                    }
                    else
                    {
                        active.Add(kv.Value);
                    }
                }
                if (expired != null)
                {
                    for (int i = 0; i < expired.Count; i++) _devices.Remove(expired[i]);
                }
            }
            return active;
        }

        public int Count { get { lock (_lock) { return _devices.Count; } } }

        /// <summary>The twelve-character lowercase-hex identity format owned by Domain & Data.
        ///
        /// Validated here rather than trusted, because this string arrives from the network. It is
        /// also a dictionary key, so an unbounded or oddly-shaped value would be a memory-growth
        /// path for anything on the LAN that can send a datagram.</summary>
        public static bool IsValidDeviceId(string id)
        {
            if (id == null || id.Length != 12) return false;
            for (int i = 0; i < id.Length; i++)
            {
                char c = id[i];
                bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
                if (!hex) return false;
            }
            return true;
        }
    }
}
