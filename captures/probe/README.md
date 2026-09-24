# Probe captures

Drop the rig's `cyd-probe` output here — unzipped, so the files sit directly in this folder.
**Two of them are deliberately not committed**; see below.

    captures/probe/properties-inventory.txt
    captures/probe/properties-inventory-early.txt
    captures/probe/samples.ndjson
    captures/probe/summary.txt
    captures/probe/probe-log.txt

This is **evidence, not source**. It is what closes Integrations' check `X1` — every row of the
iRacing mapping table observed once against a live session — and it settles the two questions that
research could not: whether the spotter fields count cars or flag presence, and whether iRacing's
per-car shift-light RPMs are actually populated.

**`samples.ndjson` and both `properties-inventory*.txt` are gitignored.**

The frame capture is excluded because it is large and regenerable. The property inventories are
excluded because they carry **personal data**: they dump every property with its live value, and
iRacing's session data names every driver on track and carries their customer IDs. Third-party
plugins republish those names under their own property trees, and the telemetry-file path contains
the operator's account name.

Redacting that reliably would mean enumerating fields that plugins are free to name however they
like. It was attempted twice and was wrong both times — once too narrow, missing `DriverUserID` and
every `benofficial2.*.Name`; once so broad it would have destroyed the evidence it was protecting.

Nothing is lost. `FINDINGS.md` records every mapping row with its observed value and the analysis
behind it, and `summary.txt` records the distinct-value answers. Those are what the mapping table in
`docs/integrations-and-external-dependencies.md` cites. Keep the raw inventories locally if you want
them; they are regenerable by re-running the probe.
