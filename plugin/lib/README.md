# SimHub reference assemblies — **not** part of this repository

Three DLLs from a local SimHub installation belong here:

    GameReaderCommon.dll
    SimHub.Logging.dll
    SimHub.Plugins.dll

They are **referenced at build time only**, with `Private="False"`, so they are never copied into
build output and never appear in a release artefact. They are gitignored, and `POLICY-NO-REDISTRIBUTION-SIMHUB`
forbids committing or shipping them. SimHub is proprietary; every builder supplies their own copies
from their own installation.

Default source on a standard install:

    C:\Program Files (x86)\SimHub\
