# Persistent storage reservation

`sce_sys/param.json` uses `downloadDataSize` in MiB, as a reservation for
`/download0`, not as the size of the app folder or a byte count. CD Player
reserves **256 MiB**. Build and packaging checks enforce this quota.

The previous 16384 value requested 16 GiB. The tested console allocated an
18,763,546,624-byte `download0.dat` including filesystem overhead. Merely
changing the package metadata does not shrink an existing console allocation.

With filesystem elevation available, the player keeps metadata, artwork,
lyrics, settings and logs in `/data/cdplayer-PPSA98427`. `/download0/cdplayer`
is the sandbox fallback. Individual artwork reads allow up to 8 MiB per image;
the reservation accommodates a disc's artwork, metadata and lyrics without
allocating 16 GiB. It is not a promise of unlimited cached albums: there is no
global cache pruning policy in this version.

Preserve application versions when correcting this metadata. No executable
rebuild is needed. Historical backups and published ZIPs are not silently
rewritten; current folder checksum manifests must match their updated param.

The user verified successful console launch at 256 MiB after the 128 MiB
configuration failed before sandbox creation (CE-104697-7). This is an
observed working quota, not a claim of a documented system minimum.
