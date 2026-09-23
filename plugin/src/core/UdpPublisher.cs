// COMPONENT-PUBLISHER — owns the UDP socket.
//
// It does three things: listens for registrations, maintains the device table, and unicasts each
// frame to every device currently registered. Optionally it also writes a capture.
//
// Two properties are worth stating because they are security assertions rather than incidental:
//
//   - It replies ONLY to the source address of a registration it actually received. There is no
//     configured destination and no broadcast, so the publisher cannot be induced to send traffic
//     to a third party — which is what makes SEC-NO-SUBSCRIPTION-AUTHZ ("any host on the LAN may
//     register") an acceptable position rather than a reflection amplifier.
//
//   - It never blocks the caller. DataUpdate runs inside SimHub at ~60 Hz; a send that stalls
//     would stall SimHub. Sends are fire-and-forget on an already-bound socket, and every failure
//     is swallowed after being counted, because a device that has gone away is the normal case
//     rather than an error.
//
// This type lives in core and carries no SimHub reference, so the whole publish path is testable
// against a loopback socket with no SimHub installed.

using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;

namespace CydSimDash.Core
{
    public sealed class UdpPublisher : IDisposable
    {
        /// <summary>Fixed by contract. A fixed port is what keeps the plugin configuration-free:
        /// the device knows where to register without being told, and the plugin needs to know
        /// nothing about the device at all.</summary>
        public const int Port = 47110;

        private readonly DeviceTable _devices = new DeviceTable();
        private readonly IFaultSink _faults;
        private UdpClient _socket;
        private Thread _listener;
        private volatile bool _running;
        private CaptureWriter _capture;
        private readonly object _captureLock = new object();

        // Counters rather than logs. A device dropping off mid-race is ordinary, and logging it
        // would turn a normal event into noise that hides a real one.
        private long _framesSent, _sendFailures, _registrationsAccepted, _registrationsRejected;

        public long FramesSent { get { return Interlocked.Read(ref _framesSent); } }
        public long SendFailures { get { return Interlocked.Read(ref _sendFailures); } }
        public long RegistrationsAccepted { get { return Interlocked.Read(ref _registrationsAccepted); } }
        public long RegistrationsRejected { get { return Interlocked.Read(ref _registrationsRejected); } }
        public int DeviceCount { get { return _devices.Count; } }
        public DeviceTable Devices { get { return _devices; } }

        public UdpPublisher(IFaultSink faults) { _faults = faults; }

        /// <param name="port">Overridable for tests only; the product always uses the fixed port.</param>
        public void Start(int port)
        {
            if (_running) return;
            _socket = new UdpClient(new IPEndPoint(IPAddress.Any, port));
            _running = true;
            _listener = new Thread(ListenLoop);
            _listener.IsBackground = true;    // never hold SimHub open on shutdown
            _listener.Name = "cyd-sim-dash publisher";
            _listener.Start();
        }

        public void Start() { Start(Port); }

        /// <summary>The bound port. Differs from Port only under test, where 0 asks the OS for a
        /// free one.</summary>
        public int BoundPort
        {
            get
            {
                IPEndPoint ep = _socket == null ? null : _socket.Client.LocalEndPoint as IPEndPoint;
                return ep == null ? 0 : ep.Port;
            }
        }

        private void ListenLoop()
        {
            while (_running)
            {
                try
                {
                    IPEndPoint from = new IPEndPoint(IPAddress.Any, 0);
                    byte[] data = _socket.Receive(ref from);
                    HandleDatagram(data, from);
                }
                catch (SocketException)
                {
                    if (!_running) return;    // ordinary: the socket closed under us on shutdown
                }
                catch (ObjectDisposedException)
                {
                    return;
                }
                catch (Exception ex)
                {
                    if (_faults != null) _faults.AdapterFaulted("publisher", ex);
                    return;                   // an unknown fault stops the listener, not SimHub
                }
            }
        }

        internal void HandleDatagram(byte[] data, IPEndPoint from)
        {
            Registration reg;
            if (!RegistrationParser.TryParse(data, data == null ? 0 : data.Length, out reg))
            {
                // Dropped silently and counted. Anything on the LAN can send here, so logging
                // malformed input would hand any host on the network a way to fill the disk.
                Interlocked.Increment(ref _registrationsRejected);
                return;
            }

            // A device speaking a different MAJOR version is not served. The device shows the
            // version-mismatch screen on its side; there is nothing useful to send it, and sending
            // frames it cannot parse would be worse than silence.
            if (reg.ProtocolMajor != Frame.ProtocolMajor)
            {
                Interlocked.Increment(ref _registrationsRejected);
                return;
            }

            if (_devices.Register(reg.DeviceId, from, reg.FirmwareVersion, NowMs()))
                Interlocked.Increment(ref _registrationsAccepted);
            else
                Interlocked.Increment(ref _registrationsRejected);
        }

        /// <summary>Unicast a frame to every currently-registered device.</summary>
        public void Publish(Frame frame)
        {
            if (frame == null) return;

            lock (_captureLock)
            {
                if (_capture != null) _capture.Write(frame);
            }

            if (_socket == null) return;

            List<DeviceEntry> targets = _devices.ActiveDevices(NowMs());
            if (targets.Count == 0) return;

            byte[] payload;
            try { payload = Encoding.UTF8.GetBytes(frame.ToJson()); }
            catch (Exception) { return; }

            for (int i = 0; i < targets.Count; i++)
            {
                try
                {
                    _socket.Send(payload, payload.Length, targets[i].EndPoint);
                    Interlocked.Increment(ref _framesSent);
                }
                catch (Exception)
                {
                    // Ordinary. A device that has been switched off is the common case, and it is
                    // already handled by the table's 6 s expiry.
                    Interlocked.Increment(ref _sendFailures);
                }
            }
        }

        // ---- captures ---------------------------------------------------------

        public bool IsCapturing { get { lock (_captureLock) { return _capture != null; } } }

        public void StartCapture(string path)
        {
            lock (_captureLock)
            {
                if (_capture != null) _capture.Dispose();
                _capture = new CaptureWriter(path);
            }
        }

        public int StopCapture()
        {
            lock (_captureLock)
            {
                if (_capture == null) return 0;
                int n = _capture.FramesWritten;
                _capture.Dispose();
                _capture = null;
                return n;
            }
        }

        private static long NowMs()
        {
            return (long)(DateTime.UtcNow - new DateTime(1970, 1, 1, 0, 0, 0, DateTimeKind.Utc))
                   .TotalMilliseconds;
        }

        public void Dispose()
        {
            _running = false;
            StopCapture();
            try { if (_socket != null) _socket.Close(); } catch (Exception) { }
            _socket = null;
            try { if (_listener != null) _listener.Join(500); } catch (Exception) { }
            _listener = null;
        }
    }
}
