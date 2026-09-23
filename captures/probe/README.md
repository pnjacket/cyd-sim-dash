# Probe captures

Drop the rig's `cyd-probe` output here — unzipped, so the files sit directly in this folder:

    captures/probe/properties-inventory.txt
    captures/probe/properties-inventory-early.txt
    captures/probe/samples.ndjson
    captures/probe/summary.txt
    captures/probe/probe-log.txt

This is **evidence, not source**. It is what closes Integrations' check `X1` — every row of the
iRacing mapping table observed once against a live session — and it settles the two questions that
research could not: whether the spotter fields count cars or flag presence, and whether iRacing's
per-car shift-light RPMs are actually populated.

`samples.ndjson` is gitignored (it is large and regenerable by driving again). The three small text
files are worth committing: they are the record of what was observed, and the mapping table in
`docs/integrations-and-external-dependencies.md` cites them.
