// COMPONENT-PLUGIN-CORE — the class SimHub actually loads.
//
// Deliberately thin. It owns the SimHub lifecycle and nothing else: every decision lives in
// FrameBuilder, which has no SimHub reference and is therefore testable without SimHub installed.
//
// Check R2 searches this component for any sim name or sim-specific property and must find nothing.
// The one place a title is named is AdapterRegistry, which is the composition root.
//
// The publisher is slice 6. The seam is here as IFramePublisher so that landing it is a
// constructor change rather than a rewrite; until then frames are built and dropped, which still
// exercises the whole adapter path against a live sim.

using System;
using GameReaderCommon;
using SimHub.Plugins;
using CydSimDash.Core;

namespace CydSimDash.SimHubBridge
{
    /// <summary>Where resolved frames go. Slice 6 supplies the UDP implementation.</summary>
    public interface IFramePublisher
    {
        void Publish(Frame frame);
    }

    [PluginName("CYD Sim Dash")]
    [PluginAuthor("cyd-sim-dash")]
    [PluginDescription("Publishes a normalised telemetry frame over the local network to a CYD dashboard panel.")]
    public class CydSimDashPlugin : IPlugin, IDataPlugin
    {
        public PluginManager PluginManager { get; set; }

        private readonly SimHubTelemetrySource _source = new SimHubTelemetrySource();
        private FrameBuilder _builder;
        private IFramePublisher _publisher;
        private UdpPublisher _udp;
        private LogSink _log;

        // A monotonic frame stamp. Milliseconds since plugin start rather than wall-clock: the
        // device orders frames by this value, and a wall clock that steps backwards over a DST
        // boundary or an NTP correction would make it discard good frames as stale.
        private readonly System.Diagnostics.Stopwatch _clock = new System.Diagnostics.Stopwatch();

        public void Init(PluginManager pluginManager)
        {
            PluginManager = pluginManager;
            _log = new LogSink();
            _builder = new FrameBuilder(new AdapterRegistry(), _log);
            _clock.Start();

            // The publisher binds the fixed port and listens for registrations. A failure here is
            // reported and then tolerated: a plugin that cannot open its socket should leave SimHub
            // working and the panel showing "unreachable", not refuse to load.
            try
            {
                UdpPublisher udp = new UdpPublisher(_log);
                udp.Start();
                _udp = udp;
                _publisher = new PublisherAdapter(udp);
                _log.Info("listening on udp/" + UdpPublisher.Port);
            }
            catch (Exception ex)
            {
                _log.Once("publisher-start", ex);
            }

            _log.Info("started, protocol " + Frame.ProtocolMajor + "." + Frame.ProtocolMinor);
        }

        /// <summary>Bridges the publisher to the seam. Kept explicit rather than having
        /// UdpPublisher implement IFramePublisher directly, because that interface belongs to the
        /// SimHub bridge and core must not reference it.</summary>
        private sealed class PublisherAdapter : IFramePublisher
        {
            private readonly UdpPublisher _udp;
            public PublisherAdapter(UdpPublisher udp) { _udp = udp; }
            public void Publish(Frame frame) { _udp.Publish(frame); }
        }

        public void End(PluginManager pluginManager)
        {
            _clock.Stop();
            if (_udp != null)
            {
                try { _udp.Dispose(); } catch (Exception) { }
                _udp = null;
            }
            _publisher = null;
        }

        /// <summary>Start recording frames to a capture file. Development tooling: captures feed
        /// the replay tool, which substitutes for this whole chain when no sim is running.</summary>
        public void StartCapture(string path)
        {
            if (_udp != null) _udp.StartCapture(path);
        }

        public int StopCapture()
        {
            return _udp == null ? 0 : _udp.StopCapture();
        }

        public void DataUpdate(PluginManager pluginManager, ref GameData data)
        {
            // The outermost fault boundary. FrameBuilder already contains the adapter boundary; this
            // one exists because nothing this plugin does is worth destabilising SimHub for, and a
            // plugin that throws out of DataUpdate at 60 Hz is exactly how that happens.
            try
            {
                _source.Bind(pluginManager, data);
                Frame frame = _builder.Build(_source, _clock.ElapsedMilliseconds);
                if (_publisher != null) _publisher.Publish(frame);
            }
            catch (Exception ex)
            {
                if (_log != null) _log.Once("data-update", ex);
            }
        }

        /// <summary>Slice 6 injects the UDP publisher here.</summary>
        public void SetPublisher(IFramePublisher publisher) { _publisher = publisher; }
    }
}
