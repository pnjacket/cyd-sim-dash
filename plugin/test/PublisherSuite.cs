// Slice 6 — publisher, device table, registration parsing, capture writer.
//
// The transport checks run against a REAL socket on the loopback interface rather than a mocked
// one. A publisher that works against a fake socket and not a real one is the only kind of bug
// this component can have that matters, so mocking the socket would test the wrong thing.

using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using CydSimDash.Core;

namespace CydSimDash.Test
{
    public static class PublisherSuite
    {
        private static IPEndPoint Ep(string addr, int port) { return new IPEndPoint(IPAddress.Parse(addr), port); }

        private static string Reg(string deviceId, int major, int minor, string fw)
        {
            return "{\"protocolMajor\":" + major + ",\"protocolMinor\":" + minor +
                   ",\"deviceId\":\"" + deviceId + "\",\"firmwareVersion\":\"" + fw + "\"}";
        }

        public static void Run()
        {
            RegistrationParsing();
            DeviceTableBehaviour();
            CaptureFormat();
            LoopbackRoundTrip();
        }

        // ---- EVT-REGISTRATION parsing ------------------------------------------
        private static void RegistrationParsing()
        {
            Console.WriteLine("I9  registration parsing rejects everything malformed");

            Registration r;
            byte[] good = Encoding.UTF8.GetBytes(Reg("a1b2c3d4e5f6", 1, 0, "0.1.6"));
            ConformanceSuite.Require(RegistrationParser.TryParse(good, good.Length, out r),
                                     "I9: a well-formed registration parses");
            ConformanceSuite.Require(r != null && r.DeviceId == "a1b2c3d4e5f6", "I9: device identity is read");
            ConformanceSuite.Require(r != null && r.FirmwareVersion == "0.1.6", "I9: firmware version is read");

            // Every one of these must be refused. The identity is a dictionary key reached from the
            // network, so a malformed one is a memory-growth path, not just bad data.
            string[] bad = new string[] {
                "",
                "not json at all",
                "{",
                "{}",
                "[1,2,3]",
                Reg("A1B2C3D4E5F6", 1, 0, "x"),            // uppercase hex
                Reg("a1b2c3d4e5f", 1, 0, "x"),             // 11 characters
                Reg("a1b2c3d4e5f60", 1, 0, "x"),           // 13 characters
                Reg("a1b2c3d4e5g6", 1, 0, "x"),            // non-hex character
                "{\"protocolMajor\":1,\"protocolMinor\":0}",                       // no identity
                "{\"protocolMajor\":\"one\",\"protocolMinor\":0,\"deviceId\":\"a1b2c3d4e5f6\"}",
                "{\"protocolMajor\":1,\"protocolMinor\":0,\"deviceId\":\"a1b2c3d4e5f6\",\"nested\":{\"a\":1}}",
                "{\"protocolMajor\":1,\"protocolMinor\":0,\"deviceId\":\"a1b2c3d4e5f6\",\"arr\":[1]}",
                "{\"protocolMajor\":1,\"protocolMinor\":0,\"deviceId\":\"a1b2c3d4e5f6\"",   // unterminated
                "{\"unterminated string\":\"aaa",
            };
            foreach (string s in bad)
            {
                byte[] b = Encoding.UTF8.GetBytes(s);
                ConformanceSuite.Require(!RegistrationParser.TryParse(b, b.Length, out r),
                                         "I9: refuses " + Describe(s));
            }

            // Oversized input is refused before parsing begins.
            byte[] huge = new byte[RegistrationParser.MaxBytes + 1];
            for (int i = 0; i < huge.Length; i++) huge[i] = (byte)'{';
            ConformanceSuite.Require(!RegistrationParser.TryParse(huge, huge.Length, out r),
                                     "I9: refuses a payload over the size bound");

            // The size bound, isolated. The case above is refused as malformed JSON whether or not
            // the bound exists, so on its own it proves nothing about the bound — mutation testing
            // showed removing the bound changed nothing. This payload is WELL-FORMED and would
            // parse happily; only the byte limit stops it, which is the property being asserted.
            StringBuilder big = new StringBuilder();
            big.Append("{\"protocolMajor\":1,\"protocolMinor\":0,\"deviceId\":\"a1b2c3d4e5f6\"");
            for (int i = 0; i < 8; i++)
            {
                big.Append(",\"pad").Append(i).Append("\":\"").Append(new string('p', 60)).Append('"');
            }
            big.Append('}');
            byte[] fatButValid = Encoding.UTF8.GetBytes(big.ToString());
            ConformanceSuite.Require(fatButValid.Length > RegistrationParser.MaxBytes,
                                     "I9: (the oversize probe really is oversized)");
            ConformanceSuite.Require(!RegistrationParser.TryParse(fatButValid, fatButValid.Length, out r),
                                     "I9: refuses a well-formed payload that exceeds the size bound");

            // A long firmware string is bounded, because it is carried into the device table.
            string longFw = new string('x', 500);
            byte[] fat = Encoding.UTF8.GetBytes(Reg("a1b2c3d4e5f6", 1, 0, longFw));
            ConformanceSuite.Require(!RegistrationParser.TryParse(fat, fat.Length, out r),
                                     "I9: refuses an unbounded firmware string");
        }

        private static string Describe(string s)
        {
            if (s.Length == 0) return "an empty payload";
            return s.Length > 46 ? s.Substring(0, 46) + "..." : s;
        }

        // ---- the device table ---------------------------------------------------
        private static void DeviceTableBehaviour()
        {
            Console.WriteLine("I8  the device table is idempotent and forgets quiet devices");

            DeviceTable t = new DeviceTable();
            long now = 1000;

            ConformanceSuite.Require(t.Register("a1b2c3d4e5f6", Ep("192.168.1.10", 5000), "0.1", now),
                                     "I8: a valid registration is recorded");
            ConformanceSuite.Require(t.Count == 1, "I8: one device after one registration");

            // Repeats refresh rather than duplicate — the check the contract names explicitly.
            for (int i = 0; i < 20; i++)
                t.Register("a1b2c3d4e5f6", Ep("192.168.1.10", 5000), "0.1", now + i * 2000);
            ConformanceSuite.Require(t.Count == 1, "I8: twenty repeats still produce one entry");

            // A device whose address moves updates in place rather than being served twice.
            t.Register("a1b2c3d4e5f6", Ep("192.168.1.99", 5000), "0.1", now);
            ConformanceSuite.Require(t.Count == 1, "I8: a moved device updates in place");
            List<DeviceEntry> active = t.ActiveDevices(now);
            ConformanceSuite.Require(active.Count == 1 && active[0].EndPoint.Address.ToString() == "192.168.1.99",
                                     "I8: frames follow the device to its new address");

            // A malformed identity records nothing.
            ConformanceSuite.Require(!t.Register("nope", Ep("192.168.1.11", 5000), "0.1", now),
                                     "I9: a malformed identity is refused");
            ConformanceSuite.Require(t.Count == 1, "I9: a refused registration adds no entry");

            // Expiry: three missed keepalives, not one. A device is still served at 5 s.
            t.Register("a1b2c3d4e5f6", Ep("192.168.1.10", 5000), "0.1", 10000);
            ConformanceSuite.Require(t.ActiveDevices(15000).Count == 1,
                                     "table: a device 5 s quiet is still served (a stall is not a departure)");
            ConformanceSuite.Require(t.ActiveDevices(17000).Count == 0,
                                     "table: a device 7 s quiet is forgotten");
            ConformanceSuite.Require(t.Count == 0, "table: the expired entry is removed, not merely hidden");
        }

        // ---- OUT-CAPTURE / ENTITY-CAPTURE ---------------------------------------
        private static void CaptureFormat()
        {
            Console.WriteLine("D9  a capture is offset-plus-frame and carries nothing classified");

            string path = Path.Combine(Path.GetTempPath(), "cyd-capture-test.ndjson");
            ITitleAdapter a = new CydSimDash.Core.Adapters.IRacingAdapter();

            using (CaptureWriter w = new CaptureWriter(path))
            {
                FakeTelemetry t = FakeTelemetry.Captured();
                w.Write(a.BuildFrame(t, 5000));
                w.Write(a.BuildFrame(t, 5100));
                w.Write(a.BuildFrame(t, 5250));
                ConformanceSuite.Require(w.FramesWritten == 3, "capture: three frames written");
            }

            string[] lines = File.ReadAllLines(path);
            ConformanceSuite.Require(lines.Length == 3, "capture: one line per frame");

            // Offsets are relative to the first frame, so a capture replays the same regardless of
            // when in the session recording started.
            ConformanceSuite.Require(lines[0].Contains("\"offsetMs\":0"), "capture: first frame is offset 0");
            ConformanceSuite.Require(lines[1].Contains("\"offsetMs\":100"), "capture: second frame offset 100");
            ConformanceSuite.Require(lines[2].Contains("\"offsetMs\":250"), "capture: third frame offset 250");
            ConformanceSuite.Require(lines[0].Contains("\"frame\":{"), "capture: carries a verbatim frame object");

            // INV-CAPTURE-CLEAN, proven the way check D9 specifies: search a real capture for each
            // classified field. It holds by construction — the writer can only serialise a Frame,
            // and none of these appears in one — but a check that relies on "by construction"
            // without looking is how by-construction claims stop being true.
            string whole = string.Join("\n", lines);
            string[] mustNotAppear = new string[] {
                "ssid", "SSID", "password", "passphrase", "credential",
                "pcHost", "deviceId", "192.168", "firmwareVersion",
            };
            foreach (string needle in mustNotAppear)
            {
                ConformanceSuite.Require(whole.IndexOf(needle, StringComparison.Ordinal) < 0,
                                         "D9: a capture contains no '" + needle + "'");
            }

            try { File.Delete(path); } catch (Exception) { }
        }

        // ---- the real transport --------------------------------------------------
        private static void LoopbackRoundTrip()
        {
            Console.WriteLine("I7  a registered device receives frames; an unregistered one does not");

            using (UdpPublisher pub = new UdpPublisher(null))
            using (UdpClient device = new UdpClient(new IPEndPoint(IPAddress.Loopback, 0)))
            {
                pub.Start(0);                                  // port 0: let the OS choose
                int port = pub.BoundPort;
                ConformanceSuite.Require(port > 0, "publisher: binds a socket");

                IPEndPoint pubEp = new IPEndPoint(IPAddress.Loopback, port);
                device.Client.ReceiveTimeout = 1500;

                ITitleAdapter a = new CydSimDash.Core.Adapters.IRacingAdapter();
                Frame frame = a.BuildFrame(FakeTelemetry.Captured(), 1234);

                // Nothing registered yet: publishing must be a no-op, not an error and not a
                // broadcast. This is the assertion that the publisher has no configured
                // destination at all.
                pub.Publish(frame);
                ConformanceSuite.Require(pub.FramesSent == 0, "I7: with nothing registered, nothing is sent");

                // Register, then expect frames at the address the registration CAME FROM — the
                // publisher is never told an address, it only ever replies to one it heard.
                byte[] reg = Encoding.UTF8.GetBytes(Reg("a1b2c3d4e5f6", 1, 0, "0.1.6"));
                device.Send(reg, reg.Length, pubEp);

                bool registered = WaitFor(delegate { return pub.DeviceCount == 1; }, 2000);
                ConformanceSuite.Require(registered, "I7: the registration is accepted");

                if (registered)
                {
                    pub.Publish(frame);
                    try
                    {
                        IPEndPoint from = new IPEndPoint(IPAddress.Any, 0);
                        byte[] got = device.Receive(ref from);
                        string json = Encoding.UTF8.GetString(got);
                        ConformanceSuite.Require(json == frame.ToJson(),
                                                 "I7: the device receives the frame verbatim");
                        ConformanceSuite.Require(from.Port == port,
                                                 "I7: it arrives from the publisher's own port");
                    }
                    catch (SocketException)
                    {
                        ConformanceSuite.Require(false, "I7: the device received no frame within the timeout");
                    }
                }

                // A registration with the wrong MAJOR version is refused: there is nothing useful
                // to send a device that cannot parse our frames.
                long before = pub.RegistrationsRejected;
                byte[] wrongMajor = Encoding.UTF8.GetBytes(Reg("b1b2c3d4e5f6", 9, 0, "0.1.6"));
                device.Send(wrongMajor, wrongMajor.Length, pubEp);
                WaitFor(delegate { return pub.RegistrationsRejected > before; }, 2000);
                ConformanceSuite.Require(pub.RegistrationsRejected > before,
                                         "I6: a mismatched major version is refused");
                ConformanceSuite.Require(pub.DeviceCount == 1,
                                         "I6: the refused device is not added to the table");

                // Junk on the port is dropped silently and counted.
                long rejectedBefore = pub.RegistrationsRejected;
                byte[] junk = Encoding.UTF8.GetBytes("this is not a registration");
                device.Send(junk, junk.Length, pubEp);
                WaitFor(delegate { return pub.RegistrationsRejected > rejectedBefore; }, 2000);
                ConformanceSuite.Require(pub.RegistrationsRejected > rejectedBefore,
                                         "I10: junk on the port is counted and dropped");
                ConformanceSuite.Require(pub.DeviceCount == 1, "I10: junk adds no device");
            }
        }

        private static bool WaitFor(Func<bool> condition, int timeoutMs)
        {
            int waited = 0;
            while (waited < timeoutMs)
            {
                if (condition()) return true;
                Thread.Sleep(25);
                waited += 25;
            }
            return condition();
        }
    }
}
