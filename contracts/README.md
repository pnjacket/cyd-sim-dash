# Wire contract

The hard boundary of the product: two components, in different languages, on different machines,
agreeing only on what crosses the wire. A third — the Python replay tool — must agree too, which is
why these fixtures are shared rather than duplicated per side.

Authoritative prose lives in [`../docs/interfaces-and-contracts.md`](../docs/interfaces-and-contracts.md).
These files are its machine-checkable projection. If they disagree, the doc wins and a schema is
wrong.

| File | Contract |
|---|---|
| `frame.schema.json` | `EVT-FRAME` — telemetry, PC → device |
| `registration.schema.json` | `EVT-REGISTRATION` — announcement and keepalive, device → PC |
| `fixtures/valid/` | Frames every implementation must accept |
| `fixtures/invalid/` | Frames every implementation must reject, each named for the rule it breaks |

## Why `additionalProperties` is true

The version rule is major-must-match, minor-tolerated: a device ignores fields it does not know, so
adding a field is a minor bump that older firmware survives. A schema that rejected unknown fields
would contradict that rule and make every field addition a breaking change.

## Rejecting is a contract too

`fixtures/invalid/` is not a courtesy. Six rows of the error catalogue name the replay tool as their
forcing mechanism, and these are the payloads it sends. A build that accepts one of them has broken
a contract just as surely as one that rejects a valid frame.
