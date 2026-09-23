// CONFORMANCE-ADAPTER — the seven checks every adapter passes, present and future.
//
// Framework-free, matching the firmware's unit tier: no test runner to install, no package to
// restore, and it builds with nothing but the Framework compiler. Exit code 0 is a pass.
//
// The suite takes an ITitleAdapter, not the iRacing adapter specifically. That is the point: a v2
// adapter passes the same checks or it does not ship, and Q10 requires a deliberately broken stub
// adapter to fail each of the seven.

using System;
using System.Collections.Generic;
using CydSimDash.Core;
using CydSimDash.Core.Adapters;

namespace CydSimDash.Test
{
    public sealed class RecordingSink : IFaultSink
    {
        public readonly List<string> Faults = new List<string>();
        public void AdapterFaulted(string titleId, Exception ex) { Faults.Add(titleId + ": " + ex.Message); }
    }

    public static class ConformanceSuite
    {
        private static int _checks;
        private static int _failures;

        public static void Require(bool condition, string label)
        {
            _checks++;
            if (!condition)
            {
                _failures++;
                Console.WriteLine("  FAIL  " + label);
            }
        }

        public static int Main(string[] args)
        {
            ITitleAdapter adapter = new IRacingAdapter();
            Console.WriteLine("CONFORMANCE-ADAPTER against " + adapter.GetType().Name);

            C1_SingleIdentity(adapter);
            C2_ThresholdOrdering(adapter);
            C3_UnavailableNotCoerced(adapter);
            C4_WireContract(adapter);
            C5_FallbackChain(adapter);
            C6_PerElementDegradation(adapter);
            C7_DoesNotThrow(adapter);
            SpotterDerivation();
            CoreStatusLadder();
            PublisherSuite.Run();

            // C4 properly: emit every frame this suite produced so the SAME validator the shared
            // fixtures run can check them. A string-contains test in C# cannot enforce a schema,
            // and this plugin cannot acquire a JSON Schema library, so the check crosses the
            // language boundary rather than being weakened to fit.
            if (args != null && args.Length == 2 && args[0] == "--emit")
            {
                EmitCorpus(adapter, args[1]);
                Console.WriteLine("emitted frame corpus to " + args[1]);

                // A capture written by the real writer, for tools/check_capture_roundtrip.py to
                // read with the Python replay tool. That crossing is the only thing that proves
                // the two serialisers agree; asserting it in C# alone would prove only that C#
                // agrees with itself.
                string capturePath = args[1].Replace(".ndjson", "") + "-capture.ndjson";
                EmitCapture(adapter, capturePath);
                Console.WriteLine("emitted capture to " + capturePath);
            }

            Console.WriteLine();
            Console.WriteLine(_failures == 0
                ? string.Format("PASS  {0} checks", _checks)
                : string.Format("FAIL  {0} of {1} checks", _failures, _checks));
            return _failures == 0 ? 0 : 1;
        }

        /// <summary>Every frame shape the adapter and core can produce, for schema validation.</summary>
        private static void EmitCorpus(ITitleAdapter a, string path)
        {
            List<Frame> frames = new List<Frame>();

            frames.Add(a.BuildFrame(FakeTelemetry.Captured(), 1));                       // the live baseline
            frames.Add(a.BuildFrame(new FakeTelemetry(), 2));                            // everything unavailable
            frames.Add(a.BuildFrame(FakeTelemetry.Captured().Remove("Gear"), 3));        // one element degraded
            frames.Add(a.BuildFrame(FakeTelemetry.Captured().Remove(KeyClr), 4));        // proximity unavailable
            frames.Add(a.BuildFrame(FakeTelemetry.Captured()
                .Remove(KeySlFirst).Remove(KeySlShift), 5));                             // fallback rung 2
            frames.Add(a.BuildFrame(FakeTelemetry.Captured().Set("Rpms", -1.0), 6));     // negative rpm rejected

            for (int v = 0; v <= 6; v++)
                frames.Add(a.BuildFrame(FakeTelemetry.Captured().Set(KeyClr, v), 10 + v));

            // The non-live statuses, which must carry no telemetry at all.
            frames.Add(Frame.Blank(Status.NoSim, null, 20));
            frames.Add(Frame.Blank(Status.UnsupportedTitle, null, 21));
            frames.Add(Frame.Blank(Status.AdapterFault, a.TitleId, 22));

            System.Text.StringBuilder sb = new System.Text.StringBuilder();
            foreach (Frame f in frames) sb.Append(f.ToJson()).Append('\n');
            System.IO.File.WriteAllText(path, sb.ToString(), new System.Text.UTF8Encoding(false));
        }

        /// <summary>A capture written by the production CaptureWriter, simulating a short rev
        /// sweep so the file resembles a real recording rather than a constant.</summary>
        private static void EmitCapture(ITitleAdapter a, string path)
        {
            using (CaptureWriter w = new CaptureWriter(path))
            {
                // Idle, then a sweep to the limiter, with a car appearing alongside part way up —
                // roughly the shape of the on-track script the capture of 2026-09-22 followed.
                for (int i = 0; i < 40; i++)
                {
                    double rpm = 1000.0 + i * 160.0;
                    int clr = (i > 12 && i < 22) ? 2 : (i >= 22 && i < 28) ? 4 : 1;
                    FakeTelemetry t = FakeTelemetry.Captured()
                        .Set("Rpms", rpm)
                        .Set("Gear", (1 + (i / 8)).ToString(System.Globalization.CultureInfo.InvariantCulture))
                        .Set(KeyClr, clr);
                    w.Write(a.BuildFrame(t, 5000 + i * 50));
                }
            }
        }

        private const string KeySlFirst = "DataCorePlugin.GameRawData.SessionData.DriverInfo.DriverCarSLFirstRPM";
        private const string KeySlShift = "DataCorePlugin.GameRawData.SessionData.DriverInfo.DriverCarSLShiftRPM";
        private const string KeyClr = "DataCorePlugin.GameRawData.Telemetry.CarLeftRight";

        // ---- C1 ---------------------------------------------------------------
        private static void C1_SingleIdentity(ITitleAdapter a)
        {
            Console.WriteLine("C1  declares one identity, selected for that title and no other");
            Require(!string.IsNullOrEmpty(a.TitleId), "C1: declares a non-empty titleId");

            AdapterRegistry reg = new AdapterRegistry();
            Require(reg.Select("IRacing") != null, "C1: selects for the observed game name IRacing");
            Require(reg.Select("IRacing") is IRacingAdapter, "C1: selects the iRacing adapter");
            Require(reg.Select("Assetto Corsa") == null, "C1: does not claim an unrelated title");
            Require(reg.Select("") == null, "C1: does not claim an empty title");
            Require(reg.Select(null) == null, "C1: does not claim a null title");

            // The casing correction, asserted so a regression to the researched spelling is caught
            // here rather than as a silent unsupportedTitle on the rig.
            Require(a.Matches("IRacing"), "C1: matches the literal observed casing");

            // Two DIFFERENT strings, pinned separately because conflating them is exactly how this
            // drifted. "IRacing" is what SimHub calls the game and is only ever used for matching;
            // "iracing" is our own identifier and is the only one that reaches the wire. The
            // fixtures and the replay tool had drifted to a third spelling, "iRacing", which was
            // neither.
            Require(a.TitleId == "iracing", "C1: the wire identity is the lowercase slug 'iracing'");
            Require(a.TitleId != "IRacing" && a.TitleId != "iRacing",
                    "C1: the wire identity is not the host's game name");

            // No two registered adapters may claim the same title.
            List<string> ids = new List<string>();
            foreach (ITitleAdapter x in reg.All)
            {
                Require(!ids.Contains(x.TitleId), "C1: titleId registered exactly once: " + x.TitleId);
                ids.Add(x.TitleId);
            }
        }

        // ---- C2 ---------------------------------------------------------------
        private static void C2_ThresholdOrdering(ITitleAdapter a)
        {
            Console.WriteLine("C2  never emits an inverted or equal threshold pair");

            FakeTelemetry inverted = FakeTelemetry.Captured()
                .Set(KeySlFirst, 6690.0)
                .Set(KeySlShift, 6130.0);
            Frame f = a.BuildFrame(inverted, 1);
            Require(!f.RampStartRpm.HasValue || f.RampStartRpm.Value < f.FlashRpm.Value,
                    "C2: an inverted native pair is not emitted as-is");

            FakeTelemetry equal = FakeTelemetry.Captured()
                .Set(KeySlFirst, 6500.0)
                .Set(KeySlShift, 6500.0);
            f = a.BuildFrame(equal, 2);
            Require(!f.RampStartRpm.HasValue || f.RampStartRpm.Value < f.FlashRpm.Value,
                    "C2: an equal native pair is not emitted as-is");

            f = a.BuildFrame(FakeTelemetry.Captured(), 3);
            Require(f.RampStartRpm.HasValue && f.FlashRpm.HasValue &&
                    f.RampStartRpm.Value < f.FlashRpm.Value, "C2: captured values emit ordered");
        }

        // ---- C3 ---------------------------------------------------------------
        private static void C3_UnavailableNotCoerced(ITitleAdapter a)
        {
            Console.WriteLine("C3  unavailable is null, never a negative or a substituted value");

            FakeTelemetry bare = new FakeTelemetry();      // running, but no properties at all
            Frame f = a.BuildFrame(bare, 1);
            Require(f.Gear == null, "C3: absent gear is null");
            Require(!f.Rpm.HasValue, "C3: absent rpm is null");
            Require(!f.RampStartRpm.HasValue && !f.FlashRpm.HasValue, "C3: absent shift pair is null");
            Require(f.SpotterLeft == null && f.SpotterRight == null,
                    "C3: absent proximity is null, not none");

            // The distinction the wire contract insists on: a readable enum meaning clear is
            // "none"; an unreadable one is null. Collapsing them would tell the driver the road is
            // clear when the truth is that nothing is known.
            FakeTelemetry clear = FakeTelemetry.Captured().Set(KeyClr, 1);
            f = a.BuildFrame(clear, 2);
            Require(f.SpotterLeft == Spotter.None, "C3: a clear road is none, not null");

            FakeTelemetry negative = FakeTelemetry.Captured().Set("Rpms", -5.0);
            f = a.BuildFrame(negative, 3);
            Require(!f.Rpm.HasValue, "C3: negative rpm becomes unavailable rather than passing through");
        }

        // ---- C4 ---------------------------------------------------------------
        private static void C4_WireContract(ITitleAdapter a)
        {
            Console.WriteLine("C4  emits only frames the wire contract accepts");

            Frame f = a.BuildFrame(FakeTelemetry.Captured(), 42);
            string json = f.ToJson();

            string[] keys = new string[] {
                "protocolMajor","protocolMinor","status","titleId","stamp","gear","rpm",
                "rampStartRpm","flashRpm","spotterLeft","spotterRight" };
            foreach (string key in keys)
            {
                Require(json.Contains("\"" + key + "\""), "C4: key always present: " + key);
            }
            Require(json.StartsWith("{") && json.EndsWith("}"), "C4: emits a JSON object");
            Require(f.StatusValue == Status.Live, "C4: a good source yields status live");

            string blank = Frame.Blank(Status.NoSim, null, 7).ToJson();
            Require(blank.Contains("\"gear\":null"), "C4: a blank frame carries gear as explicit null");
            Require(blank.Contains("\"titleId\":null"), "C4: a blank frame carries titleId as null");
        }

        // ---- C5 ---------------------------------------------------------------
        private static void C5_FallbackChain(ITitleAdapter a)
        {
            Console.WriteLine("C5  runs the fallback chain in order, inventing nothing");

            // Rung 1 — the sim's own values, as observed on 2026-09-22.
            Frame f = a.BuildFrame(FakeTelemetry.Captured(), 1);
            Require(f.RampStartRpm == 6130.0 && f.FlashRpm == 6690.0,
                    "C5 rung 1: native absolute values are used verbatim");

            // Rung 2 — no native values, but a redline.
            FakeTelemetry noNative = FakeTelemetry.Captured()
                .Remove(KeySlFirst).Remove(KeySlShift);
            f = a.BuildFrame(noNative, 2);
            Require(f.RampStartRpm.HasValue && Math.Abs(f.RampStartRpm.Value - 0.88 * 7500.0) < 1e-6,
                    "C5 rung 2: ramp start derived at 0.88 of redline");
            Require(f.FlashRpm.HasValue && Math.Abs(f.FlashRpm.Value - 0.97 * 7500.0) < 1e-6,
                    "C5 rung 2: flash derived at 0.97 of redline");

            // A car whose shift lights report ZEROS must fall through to rung 2, never emit 0.
            // This is the case the operator's "the capture car is not the car I drive" concern
            // surfaced, and it is the reason rung 1 requires strictly positive values.
            FakeTelemetry zeroed = FakeTelemetry.Captured()
                .Set(KeySlFirst, 0.0).Set(KeySlShift, 0.0);
            f = a.BuildFrame(zeroed, 3);
            Require(f.RampStartRpm.HasValue && f.RampStartRpm.Value > 0.0,
                    "C5: zeroed shift lights fall through to the fraction rule, never emit 0");

            // The dangerous half of that case, and the one the pair above does NOT exercise: a
            // first light of 0 with a real shift point is strictly ORDERED, so the ordering guard
            // lets it through and only the positivity guard stops it. Without this case the
            // positivity guard is untested, and removing it would silently put the ramp start at
            // 0 rpm — a permanently lit panel. Found by mutation-testing the suite.
            FakeTelemetry halfZero = FakeTelemetry.Captured()
                .Set(KeySlFirst, 0.0).Set(KeySlShift, 6690.0);
            f = a.BuildFrame(halfZero, 5);
            Require(f.RampStartRpm.HasValue && f.RampStartRpm.Value > 0.0,
                    "C5: a zero first-light with a real shift point does not emit a 0 ramp start");
            Require(f.RampStartRpm.HasValue && Math.Abs(f.RampStartRpm.Value - 0.88 * 7500.0) < 1e-6,
                    "C5: it falls through to the fraction rule rather than to unavailable");

            // Rung 3 — nothing usable at all.
            FakeTelemetry nothing = new FakeTelemetry();
            f = a.BuildFrame(nothing, 4);
            Require(!f.RampStartRpm.HasValue && !f.FlashRpm.HasValue,
                    "C5 rung 3: reports unavailable rather than inventing thresholds");
        }

        // ---- C6 ---------------------------------------------------------------
        private static void C6_PerElementDegradation(ITitleAdapter a)
        {
            Console.WriteLine("C6  a missing property degrades exactly that element");

            FakeTelemetry noGear = FakeTelemetry.Captured().Remove("Gear");
            Frame f = a.BuildFrame(noGear, 1);
            Require(f.Gear == null, "C6: removing gear nulls gear");
            Require(f.Rpm.HasValue, "C6: removing gear leaves rpm intact");
            Require(f.RampStartRpm.HasValue, "C6: removing gear leaves the shift pair intact");
            Require(f.SpotterLeft != null, "C6: removing gear leaves proximity intact");
            Require(f.StatusValue == Status.Live, "C6: removing gear does not change status");

            FakeTelemetry noSpotter = FakeTelemetry.Captured().Remove(KeyClr);
            f = a.BuildFrame(noSpotter, 2);
            Require(f.SpotterLeft == null && f.SpotterRight == null,
                    "C6: removing the enum nulls proximity");
            Require(f.Gear != null && f.Rpm.HasValue,
                    "C6: removing the enum leaves gear and rpm intact");

            // A RENAMED property is indistinguishable from a missing one, which is the point.
            FakeTelemetry renamed = FakeTelemetry.Captured().Remove("Rpms").Set("RpmsV2", 6000.0);
            f = a.BuildFrame(renamed, 3);
            Require(!f.Rpm.HasValue, "C6: a renamed rpm property nulls rpm");
            Require(f.Gear != null, "C6: a renamed rpm property leaves gear intact");
        }

        // ---- C7 ---------------------------------------------------------------
        private static void C7_DoesNotThrow(ITitleAdapter a)
        {
            Console.WriteLine("C7  never throws for input the title can legitimately produce");

            object[][] cases = new object[][] {
                new object[] { "Gear", "" },
                new object[] { "Gear", "  " },
                new object[] { "Gear", "12" },
                new object[] { "Rpms", double.NaN },
                new object[] { "Rpms", double.PositiveInfinity },
                new object[] { "MaxRpm", 0.0 },
                new object[] { "MaxRpm", -1.0 },
                new object[] { KeyClr, 99 },
                new object[] { KeyClr, -1 },
                new object[] { KeyClr, "not a number" },
            };

            foreach (object[] c in cases)
            {
                FakeTelemetry t = FakeTelemetry.Captured().Set((string)c[0], c[1]);
                try
                {
                    Frame f = a.BuildFrame(t, 1);
                    Require(f != null, "C7: returns a frame for " + c[0]);
                }
                catch (Exception ex)
                {
                    Require(false, "C7: threw for " + c[0] + " (" + ex.GetType().Name + ")");
                }
            }

            FakeTelemetry weird = FakeTelemetry.Captured().Set(KeyClr, 99);
            Frame wf = a.BuildFrame(weird, 2);
            Require(wf.SpotterLeft == null && wf.SpotterRight == null,
                    "C7: an undocumented enum value degrades to unavailable");
        }

        // ---- the spotter derivation, all seven rows -----------------------------
        //
        // Check X2. This is the whole reason for reading the raw enum: every row is asserted here,
        // including the both-sides case that cannot be staged on a real track.
        private static void SpotterDerivation()
        {
            Console.WriteLine("X2  all seven enum rows map as specified");

            object[][] table = new object[][] {
                //            value  left            right
                new object[] { 0, Spotter.None, Spotter.None },
                new object[] { 1, Spotter.None, Spotter.None },
                new object[] { 2, Spotter.One,  Spotter.None },
                new object[] { 3, Spotter.None, Spotter.One  },
                new object[] { 4, Spotter.One,  Spotter.One  },   // cars BOTH sides
                new object[] { 5, Spotter.One,  Spotter.None },   // two left, rendered as one
                new object[] { 6, Spotter.None, Spotter.One  },   // two right, rendered as one
            };

            foreach (object[] row in table)
            {
                int v = (int)row[0];
                Require(IRacingAdapter.LeftFromEnum(v) == (string)row[1],
                        "X2: enum " + v + " left = " + row[1]);
                Require(IRacingAdapter.RightFromEnum(v) == (string)row[2],
                        "X2: enum " + v + " right = " + row[2]);
            }

            Require(IRacingAdapter.LeftFromEnum(7) == null, "X2: out-of-domain left is unavailable");
            Require(IRacingAdapter.RightFromEnum(-1) == null, "X2: out-of-domain right is unavailable");

            // And end to end through the adapter, not merely through the helper.
            ITitleAdapter a = new IRacingAdapter();
            Frame both = a.BuildFrame(FakeTelemetry.Captured().Set(KeyClr, 4), 1);
            Require(both.SpotterLeft == Spotter.One && both.SpotterRight == Spotter.One,
                    "X2: a car on each side lights BOTH sides through the adapter");
        }

        // ---- the core's status ladder and fault boundary ------------------------
        private static void CoreStatusLadder()
        {
            Console.WriteLine("core  status ladder and the fault boundary");

            RecordingSink sink = new RecordingSink();
            FrameBuilder core = new FrameBuilder(new AdapterRegistry(), sink);

            FakeTelemetry stopped = FakeTelemetry.Captured();
            stopped.IsGameRunning = false;
            Require(core.Build(stopped, 1).StatusValue == Status.NoSim, "core: not running yields noSim");
            Require(core.Build(null, 2).StatusValue == Status.NoSim, "core: no source yields noSim");

            FakeTelemetry other = FakeTelemetry.Captured();
            other.GameName = "Some Other Sim";
            Frame f = core.Build(other, 3);
            Require(f.StatusValue == Status.UnsupportedTitle, "core: unknown title yields unsupportedTitle");

            // titleId is NOT null here, and the contract is explicit about why: naming the title is
            // the entire point of this status, and the device has a screen to name it on. An
            // earlier version of this returned null on a leak-prevention rationale that the
            // contract does not support; the device would have shown an empty screen.
            Require(f.TitleId == "Some Other Sim",
                    "INV-STATUS-CONSISTENT: an unsupported title carries the title's own name");
            Require(f.Gear == null && !f.Rpm.HasValue,
                    "INV-STATUS-CONSISTENT: but it still carries no telemetry");

            // An over-long name is bounded to what the device's fixed field can hold, so the wire
            // carries what will actually be shown rather than something silently truncated later.
            FakeTelemetry longName = FakeTelemetry.Captured();
            longName.GameName = new string('X', 200);
            Frame lf = core.Build(longName, 31);
            Require(lf.TitleId != null && lf.TitleId.Length <= 31,
                    "core: an over-long title name is bounded to the device's field width");

            Require(core.Build(FakeTelemetry.Captured(), 4).StatusValue == Status.Live,
                    "core: a known title with good data yields live");

            // An adapter that throws must not take SimHub down, and must log ONCE rather than on
            // every update — flooding the log is the failure the boundary exists to prevent, so it
            // must not become the boundary's own failure mode.
            FakeTelemetry boom = FakeTelemetry.Captured();
            boom.Explosive.Add("Gear");
            FrameBuilder core2 = new FrameBuilder(
                new AdapterRegistry(new ITitleAdapter[] { new ThrowingAdapter() }), sink);
            sink.Faults.Clear();
            Frame a1 = core2.Build(boom, 5);
            Frame a2 = core2.Build(boom, 6);
            Frame a3 = core2.Build(boom, 7);
            Require(a1.StatusValue == Status.AdapterFault, "core: a throwing adapter yields adapterFault");
            Require(a2.StatusValue == Status.AdapterFault, "core: it keeps yielding adapterFault");
            Require(sink.Faults.Count == 1,
                    "core: three faulting updates log once, not three times (got " + sink.Faults.Count + ")");
            Require(a3.Gear == null && !a3.Rpm.HasValue, "core: a faulted frame carries no stale telemetry");

            // Recovery: once the source stops exploding, the adapter returns to live.
            Require(core2.Build(FakeTelemetry.Captured(), 8).StatusValue == Status.Live,
                    "core: a recovered adapter returns to live");
        }

        private sealed class ThrowingAdapter : ITitleAdapter
        {
            public string TitleId { get { return "iracing"; } }
            public bool Matches(string gameName) { return true; }
            public Frame BuildFrame(ITelemetrySource source, long stamp)
            {
                string g;
                source.TryGetString("Gear", out g);       // explodes when Gear is marked explosive
                return Frame.Blank(Status.Live, TitleId, stamp);
            }
        }
    }
}
