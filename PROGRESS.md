# Launcher_CSNZ Progress

Last updated: 2026-07-11

## 2026-07-16 Latest hw.dll Signature Audit

- Rechecked the signature block in `hook.cpp` against
  `asset/Bin/hw.dll` in Ghidra (image base `0x01D00000`).
- Found the client shutdown root cause:
  - `NGCLIENT_INIT_SIG_CSNZ` still has one match in the latest DLL.
  - That match is not NGClient initialization. It is the engine startup call
    to `BlackCipher_Init` at RVA `0x7BC0BF`, targeting RVA `0x88BD50`.
  - Replacing that call with `NGClient_Return1` skipped BlackCipher queue and
    critical-section initialization. The frame loop later entered the
    uninitialized critical section and terminated with `0xC0000005`.
- The launcher now resolves the matched `CALL` target and refuses the legacy
  NGClient hook when it targets the required BlackCipher initializer.
- The old fixed-distance ZombieSkillProperty and FireBomb patches remain
  disabled because they overlap function instructions in this DLL.
- Latest-DLL semantic checks completed:
  - Socket manager constructor: valid, RVA `0xA79450`.
  - Server connect call: valid, RVA `0x96BD4A`, target RVA `0xA79CB0`.
  - HolePunch get/set server info: valid, RVAs `0x7C9430` / `0x7C9D70`.
  - CreateStringTable: valid, RVA `0xA7E980`; its parser call remains at
    function offset `+0x71`.
  - Latest LoadJson/file-buffer function: valid, RVA `0x8D5830`.
  - LogToErrorLog, ReadPacket call, GetSSLProtocolName call, and latest socket
    constructor call were all confirmed at their matched locations.
  - NGClient quit, packet hack send/parse, alarm parser, bot manager pointer,
    and CSOMainPanel pointer signatures do not match this DLL and remain
    optional legacy paths.
- Full default runtime test (no metadata filter/minimal mode):
  - Release launcher built and deployed to `asset/Bin/CSOLauncher.exe`.
  - Client and server were both alive after 60 seconds.
  - Server reached Login(3), sent the full initial metadata set, received the
    crypt acknowledgement, joined channel `1-1`, and exchanged Lobby(153) and
    GameMatchRoomList(151) traffic.
  - Test log: `CSNZ_Server/bin/full_rootfix_20260716_114756.stdout.txt`.
- The vectored exception stack logger is now enabled only by `-dumpall`, since
  BlackCipher and NGClient use handled first-chance exceptions during normal
  startup.

## Current Goal

Run the latest CSNZ launcher against the local `CSNZ_Server` without relying on the GUI login flow, then trace the latest `hw.dll` login/bootstrap packet flow until lobby entry works.

## Implemented

- Added command-line login support.
  - Supported options: `-login` / `-id`, `-password` / `-pw`.
  - The launcher can auto-submit the login dialog after startup.
- The auth hook now validates CLI credentials against the local server database before forcing local game-server auth.
- Fixed an earlier crash source by allocating login/password strings through the latest `hw.dll` internal string allocator before calling the original auth function.
- Added runtime tracing to `LauncherTrace.log`.
  - Launcher command line and auth state.
  - `HWSocketManager::Connect`.
  - `ReadPacket` result, packet id, handler, vtable, parser RVA.
  - Latest login socket handler table.
- Confirmed latest login socket initially registers only these handlers:
  - `id=0`, parser RVA `0x009A2750`
  - `id=1`, parser RVA `0x00985EC0`
  - `id=7`, parser RVA `0x00960500`
  - `id=81`, parser RVA `0x00968E70`

## Important Findings

- Login GUI bypass is working: the launcher can submit `localuser/localpass1` from CLI.
- The latest `hw.dll` login socket does not accept the old server's legacy post-login packets such as `123`, `150`, and `157` at this stage.
- After successful `id=1` login reply parsing, the latest socket adds handler `id=109`.
- Server-side `Help(109)` with one zero byte parses successfully, but does not yet advance the client to the crypt acknowledgement.
- `id=7` is the crypt/bootstrap parser.
  - It reads `type` then `method`.
  - For method `2/3/4`, key/iv length is selected by SSL protocol name:
    - `TLSv1`: 32 bytes key + 32 bytes iv.
    - Other protocol names: 64 bytes key + 64 bytes iv.
- Method `2` descriptor in the latest `hw.dll` uses RC4-compatible stream handling with a 16-byte effective key length.
- The latest client now parses both crypt packets and sends plaintext acknowledgement packet `id=12`.
- `GetSSLProtocolName` is now hooked even when SSL mode is enabled.
  - The hook returns `TLSv1`.
  - Latest trace confirms the hook is called while parsing `Packet_Crypt_Parse`.
- `ReadPacket result=8` has been identified in `hw.dll`.
  - It is a `U` packet sequence mismatch.
  - `hw.dll` stores the last sequence at socket offset `0x39` and expects the next server packet sequence to increment by one.
- Added `HWSocketReadState` tracing for `ReadPacket` failures.
  - After crypt type `0`, the latest client sets input-crypt flags (`+0x1c`, `+0x44`).
  - On crypt type `1`, the internal read pointer advances across the incoming bytes but no decrypted `U` header is found.
- Observed client behavior:
  - `method=4`, 64-byte key/iv: type 0 parses, type 1 does not progress, client later closes.
  - `method=2`, 32-byte key/iv with server-side RC4 output enabled before type 1: type 0 parses, then `ReadPacket result=8`.
  - `method=2`, 32-byte key/iv plus `Help(109)`: `Help(109)` and type 0 parse, type 1 is not acknowledged, client later closes.
  - With server sequence fixed (`seq=3 -> seq=4`), `result=8` is gone, but type `1` still does not decrypt/parse.
- After the crypt acknowledgement, sending old lobby/bootstrap packets (`123`, `150`, `157`, `91`, `99`, `159`) to the same socket is still ignored because the active handler table is auth-oriented.
- `id=81` parser was identified as `HostServer`.
  - subtype `0`: logs `HostServer Stop` and quits.
  - subtype `1`: logs `HostServer Transfer`, sleeps, reads `ip + port`, and opens a new connection.
- Runtime confirms `HostServer Transfer(81/1)` is accepted:
  - the client closes the first auth socket;
  - after roughly three seconds it opens a second TLS connection to the transferred endpoint;
  - the second connection sends `HostServer AddServer(81/0)` and `HostServer Unk3(81/3)`.
- On the second connection, metadata packets sent too early are ignored until the metadata handler is registered, so server metadata is now delayed until `81/3`.
- Latest metadata packets require a chunk flag byte after `metadataID`.
  - Server-side `metadataID + uint16 size + payload` caused `[METADATA] Received wrong flag (resource/MapList.csv)`.
  - Single zip/blob metadata is now sent as `metadataID + 0x05 + uint16 size + payload`.
- Latest metadata parse observations:
  - `0 resource/MapList.csv`, `12 Matching.csv`, `21 MileageShop.csv`, and `27 resource/GameModeList.csv` parse with `result=1`.
  - `30 resource/zombiez/progress_unlock.csv`, `31 ReinforceMaxLv.csv`, and `32 ReinforceMaxEXP.csv` were remapped against the latest parser and re-enabled server-side.
  - `39 HonorShop.csv` caused a parse call without a matching leave/result and remains disabled server-side.
- The server now emits `TX latest metadata: id=..., name=...` before each metadata packet so launcher/server logs can be lined up directly.
- Official capture `official_game_wireshark_222_122_48_21.pcapng` was parsed with `python-pcapng`.
  - It confirms a dense post-login server-to-client burst of roughly `368 KB`.
  - The payload is encrypted/custom-crypted, so the capture is mainly useful for timing and transfer-volume comparison unless the stream keys are recovered.
- `-dumpmetadata` was updated for the latest metadata packet layout:
  - old layout: `metadataID + uint16 size + payload`
  - latest layout: `metadataID + chunkFlag + uint16 size + payload`
  - ZIP dumps now write only the ZIP payload and use flat safe filenames under `MetadataDump`.
- `C:\Users\user\Desktop\sslkey.log` was checked against the official pcap.
  - It does not contain the game server flow Client Random `fdd2cd7998e05043baea42cae173504f6cb63731d2568302b7abcae4fd11f8d4`.
  - It only matches unrelated HTTPS flows, so it cannot decrypt `222.122.48.21:8001`.
- The rebuilt launcher was copied to `asset\Bin\CSOLauncher.exe`.

## Open Questions

- Which packet the host-server connection expects after metadata.
- Whether `VoxelURLs(103)` belongs later in the host-server bootstrap or only in a different client state.
- At what point the full lobby/game handler table is registered on the latest client.
- Whether the remaining enabled metadata entries after `35 item.csv` all return `result=1` with the latest `hw.dll`.
- The latest `Lobby(153)` and `GameMatchRoomList(151)` parsers have been
  partially mapped. The server now sends a minimal self lobby entry and minimal
  room-list entries; richer user fields and room detail fields are still
  deferred until their latest layouts are mapped.
- Whether an official-server live run with `-dumpmetadata` can recover the real latest metadata files after client-side decryption.

## How To Test

Run the launcher from `asset\Bin`:

```powershell
Start-Process -FilePath 'D:\project\CSONLINE\asset\Bin\CSOLauncher.exe' -WorkingDirectory 'D:\project\CSONLINE\asset\Bin' -ArgumentList '-game cstrike-online -login localuser -password localpass1'
```

Check:

```powershell
Get-Content -LiteralPath 'D:\project\CSONLINE\asset\Bin\LauncherTrace.log' -Tail 220
```

Metadata triage:

```powershell
Get-Content -LiteralPath 'D:\project\CSONLINE\asset\Bin\LauncherTrace.log' -Tail 500 |
  Select-String -Pattern 'Packet_Metadata_Parse|ReadPacket this=.* id=91|HWSocketReadState'
```

Official metadata dump path, when running a decrypting client session:

```powershell
CSOLauncher.exe -game cstrike-online -lang kr -dumpmetadata
```

Dumped ZIP payloads are written to `asset\Bin\MetadataDump`.

## 2026-07-16 Current Locale And GUI Verification

- Asset `hw.dll` language parsing at `0x0264ED20` recognizes `kr`; `ko_` is
  absent from its language table and leaves the locale name empty.
- The launcher now defaults to `-lang kr` and normalizes an explicitly supplied
  legacy `-lang ko_` to `kr` before loading `hw.dll`.
- A user-desktop launch created visible window `0x290B5C` titled
  `Counter-Strike Online PID(26508)`. `CVideoMode_OpenGL display activate`,
  the login dialog constructor, and `GameUI_RunFrame` all ran successfully.
- Asset `hw.dll` uses `-lang kr` to mount `/lstrike/locale_kr/`. Adding
  `-language koreana` alone did not populate the current VGUI localization
  table; the earlier Korean HUD screenshot was texture content and was not
  valid evidence of localization.
- Runtime inspection through `VGUI_Localize003` proved that
  `CSO_ItemInventoryTitle` was absent even though
  `Resource/cso_koreana.txt` could be opened. The launcher now explicitly
  loads the six Korean Valve/CSO localization files after `gameui.dll` loads
  and before its first frame.
- All six `AddFile` calls returned success. A subsequent token lookup returned
  `CSO_ItemInventoryTitle = 인벤토리`, while the client remained responsive
  through login and lobby entry.

## 2026-07-16 Final Main UI And Locale Status

This section supersedes the localization conclusion immediately above.

- The latest integrated run reaches login, UDP bootstrap, `Lobby(153)`, and
  `GameMatchRoomList(151)`. The main lobby UI is therefore no longer blocked
  by missing lobby or room-list initialization packets.
- An empty room list is currently expected because no dedicated server has
  successfully registered and no room has been created.
- `-lang kr` remains the correct asset-engine language option. `ko_` is not a
  valid language-table entry in the loaded `hw.dll`.
- Loading the six localization files from `HookThread` is not safe. Although
  each `AddFile` call can return success and a token can briefly resolve, the
  call races `gameui.dll` panel construction. Depending on timing it either
  leaves `#CSO_*` placeholders or causes an access violation.
- The worker-thread localization repair call is now disabled. The automated
  lobby run remained responsive after it was disabled, and the reported
  reference-error behavior was not reproduced in that stable run.
- The remaining localization fix must run on the game/main thread at the
  original localization initialization point. Before enabling it, map the
  latest `hw.dll`/`gameui.dll` call site and preserve the engine's original
  file-load order.
- `TraceAndRepairLocalization` remains only as diagnostic/reference code; it
  is not part of the active startup path.

### Remaining Launcher Work

1. Hook or extend the original main-thread localization initialization and
   verify Korean tokens across login, inventory, lobby, and room panels.
2. Retest the client after HLDS can load its dedicated data and register with
   the master server.
3. Capture a crash stack if the `0x00A952CD`-style reference error returns;
   the current stable run did not provide a reproducible faulting stack.
