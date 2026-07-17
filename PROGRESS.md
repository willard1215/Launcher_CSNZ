# Launcher_CSNZ Progress

Last updated: 2026-07-16

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

## 2026-07-16 Ghidra Main-Thread Localization Hook

- Imported and analyzed the current `GameUI.dll` beside `hw.dll` in the
  `Ghidra_CSO` project.
- Mapped the relevant `hw.dll` startup functions and saved names/comments in
  Ghidra:
  - `0x0232D070` `BaseUI_LoadGameUIAndInterfaces`
  - `0x0232D850` `BaseUI_Start`
  - `0x028311A0` `VGUI_InitializeInterfaces`
  - `0x02831550` `VGUI_GetLocalize`
  - `0x02831500` `VGUI_GetFileSystem`
- `BaseUI_Start` is the original main-thread localization site. It loads the
  six `valve/cstrike/cso/vgui/gameui/platform` localization files before panel
  creation and before `GameUI007::Initialize`.
- Locale IDs `2/3/4/5/7/8/9` have explicit language-file branches. The Korean
  path falls through to `Resource/*_%language%.txt`, which explains why
  `-lang kr` mounts the assets but does not by itself guarantee populated VGUI
  tokens.
- Added a guarded inline hook at `hw.dll` RVA `0x62D850`. It validates the
  current five-byte prologue, then preloads the six explicit Korean files on
  the engine/main thread immediately before the original `BaseUI_Start`.
- The worker-thread localization call remains disabled. The new hook reuses
  `TraceAndRepairLocalization` only at the mapped safe point and resets its
  one-shot guard if the engine restarts.
- Release x86 build succeeded and produced `Release/CSOLauncher.exe`; only the
  repository's existing code-page/signedness warnings remain.
- Live validation completed with the Release x86 artifact copied to
  `Counter-Strike Online/Bin/CSOLauncher.exe` (matching SHA-256
  `6F2BA880FE9064868B64A6504E71835EA72ADF1AC3B95386AB6D352CDE500554`).
- Three launches reached the mapped `BaseUI_Start` safe point. Every run logged
  matching enter/leave lines, all six Korean files returned `AddFile=1`, and
  both `CSO_ItemInventoryTitle` token lookups returned non-null values.
- The localization hook is not the later client-exit source. The default login
  sample replay sends packet index 134 as `GuideQuest(120)`, 1720 bytes; the
  latest client reports repeated `PacketReader Error` entries for that payload
  and exits roughly four seconds after bootstrap.
- Isolated server run with `CSNZ_LOGIN_SAMPLE_MAX_INDEX=133` and
  `CSNZ_ASSET_MIN_METADATA=1` excluded that payload. The deployed launcher
  remained responsive and connected for more than 50 seconds, with one logged
  in user on the server. Remaining parser warnings concern legacy packet 157
  and are independent of localization.
- `CSNZ_Server` now defaults the sample replay ceiling to index 133 while
  retaining `CSNZ_LOGIN_SAMPLE_MAX_INDEX` as an explicit diagnostic override.
  After rebuilding the server, a run without that override confirmed range
  `6..133`, no outbound `GuideQuest(120)` payload, and a responsive client for
  more than one minute. Server SHA-256:
  `196C98465D3091054FA17A236527B5C7A9EB4219D706085619FB6BFD0CDAAA01`.
- GUI automation was not used for this validation. Evidence came from process
  health plus `LauncherTrace.log`, `Error.log`, and the server connection log.

## 2026-07-16 Runtime Error Follow-up

- Corrected the earlier live-test conclusion: `Responding=True` was not enough
  to call the run successful because a modal runtime-error path can leave the
  process responsive.
- The failing run contained 154 `PacketReader Error` entries for
  `UserUpdateInfo(157)` subtype `173`. They came from captured login sample
  index 79 (`Packet_79_ID_157_471.bin`), not from the localization hook.
- `CSNZ_Server` now:
  - skips captured sample index 79 while retaining the short compatible 157
    samples at indices 117/118;
  - skips its generated legacy full `UserUpdateInfo(157)` for latest clients.
- Rebuilt server and reran the deployed launcher. The clean run replayed 127
  packets over range `6..133`, logged zero `PacketReader Error` entries, kept
  one connected/logged-in user, and remained responsive for over one minute.
- `Error.log` still reports 197 `no baseWeapon` entries during local weapon
  data initialization. They precede login-sample processing, produced no
  runtime/abort/assert text, and are tracked separately from the resolved 157
  runtime error.

## 2026-07-16 Ghidra DLL Cross-check

- Imported and auto-analyzed the current `client.dll` and `mp.dll` in the
  existing `Ghidra_CSO` project alongside `hw.dll`.
- Confirmed matching exported weapon factories in both game modules by
  creating and decompiling the `weapon_laserfist`, `weapon_laserfistex`, and
  `weapon_ak47` entry points. The client/server object sizes differ where
  expected, but the corresponding factories and class initialization paths
  are present in both DLLs.
- Located the `no baseWeapon : {}` reference in `hw.dll` at `0x02C566D4` and
  its only caller, `FUN_02565DA0`. The function builds a weapon-property table,
  resolves a derived record's base name only against records already added,
  and logs the warning when that lookup fails. This runs during local engine
  weapon initialization before game-server login and is not the packet-157
  runtime error.
- Recovered the `Packet_UserUpdateInfo` RTTI/vtable at `0x02C7A0E4`. Its parse
  virtual is `FUN_026A1840`, which reads a four-byte packet field and delegates
  to `FUN_026BD6C0` with mode `2`.
- `FUN_026BD6C0` first consumes an eight-byte/64-bit presence mask and then
  reads a variable set of fields selected by that mask. This confirms that a
  generated fixed-layout 471-byte packet is not a safe replacement for the
  captured wire format. Keeping the compatible short captures and excluding
  captured sample 79 plus the generated legacy full packet matches the actual
  client parser.
- Recovered the `Packet_Metadata` vtable at `0x02C79518`; its parser is
  `FUN_02677B30`, a subtype switch covering the streamed metadata records seen
  in `Error.log`.

## 2026-07-16 Clean Build and Integrated Runtime Test

- Rebuilt `Launcher_CSNZ` as Release x86 with zero errors and zero warnings.
  The artifact and the copy deployed under `Counter-Strike Online` have the
  same SHA-256:
  `6F2BA880FE9064868B64A6504E71835EA72ADF1AC3B95386AB6D352CDE500554`.
- The old server CMake cache referenced another source path and an unavailable
  Windows SDK. Generated a clean Win32 build tree with the installed SDK and
  rebuilt `CSNZ_Server.exe` successfully. SHA-256:
  `289C74C7779B7940ED14E54C4947903A2314E14E10763C5429D97490F05D6F4A`.
- An initial server smoke test was launched from `bin/Release` and produced a
  C++ exception while opening relative `Data/*.csv` files. Ghidra analysis of
  the new executable identified the relative-file loader; starting it from
  `bin` is the required runtime layout. This was a test working-directory
  error, not a new server-code fault.
- Ran the corrected server plus the normally windowed deployed launcher for
  60 seconds without GUI automation. The client connected to TCP port 30002,
  authentication succeeded, and the mapped `BaseUI_Start` hook executed.
- This first conclusion was incomplete: it did not launch the deployed
  launcher with `-login localuser -password localpass1`, so later checks showed
  the login metadata path had not actually been exercised.

## 2026-07-16 Runtime Metadata ID And Lobby Fix

- Re-ran the deployed `Counter-Strike Online/Bin/CSOLauncher.exe` with explicit
  local login arguments and reproduced the post-login fault:
  `hw.dll` Application Error events at offset `0x0097727c` when metadata was
  sent with latest table IDs.
- Used `CSNZ_Server/Packets_sampel` as the live Steam packet reference. Those
  `ID_91` metadata packets use legacy/wire IDs while carrying the current
  payload layouts, e.g. `weaponparts.csv` as id `17` and
  `ZBCompetitive.json` as id `48`.
- `weaponparts.csv` and `ZBCompetitive.json` both parsed and remained stable
  when sent with `CSNZ_METADATA_IDSET=legacy`. The server now defaults to that
  live wire-ID mapping; `CSNZ_METADATA_IDSET=latest` remains available as an
  explicit diagnostic override.
- Kept the latest-client guard that skips the legacy full
  `UserUpdateInfo(157)` regardless of metadata ID set, so switching metadata
  IDs does not re-enable the unsafe generated full 157 packet.
- Removed the legacy `GameMatch(99)` startup bootstrap from the latest login
  path and reduced `Lobby(153)` join to an empty latest-safe user list. This
  removed the remaining `PacketReader Error - curpacketID : 153[0], len : 4`.
- Final integrated run used the rebuilt server and deployed launcher with
  `-game cstrike-online -lang kr -login localuser -password localpass1`.
  It sent 23 metadata records, reached local login bootstrap, stayed connected
  for 60 seconds, and produced:
  `crashes=0`, `packetReader=0`, `runtimeLike=0`.
- Rebuilt the server again after the lobby fix; latest
  `CSNZ_Server/bin/Release/CSNZ_Server.exe` SHA-256 is
  `1EA07323BCED5A4D3191D4E186D997B77A3E520D73312ECAC7267D3EDA691ED4`.
  A fresh 60-second integrated run with the same deployed launcher and explicit
  login args stayed connected and produced:
  `crashes=0`, `meta=28`, `packetReader=0`, `runtimeLike=0`,
  `skip99=1`, `lobbySafe=1`.
- Follow-up metadata cleanup now mirrors the live Steam stream more closely:
  the server sends four live hash probes for wire ids `32..35`, uses a
  64000-byte metadata chunk default, loads/sends `voxel/voxel_list.csv`, and
  stops login sample replay at packet index `127` by default because packets
  `128..131` are the large metadata refresh now generated by the server.
- Final no-override integrated run used `CSNZ_Server.exe` SHA-256
  `4783C988E9A723E8610CAC9BBCED6E852985BCB403C871F25F8EA756265355AD`.
  The deployed launcher stayed connected for 60 seconds with replay
  `range=6..127`, `hashProbe=4`, `chunk64000=1`, and exactly one parsed copy
  each of `resource/item.csv`, `voxel/voxel_list.csv`, and
  `resource/codis/codisdata.cso`. Result:
  `crashes=0`, `packetReader=0`, `runtimeLike=0`.
- The log interval still contains 197 `no baseWeapon` warnings from local
  weapon-table initialization. Ghidra analysis already tied these to
  `hw.dll` weapon-property base lookup order, not to the resolved post-login
  runtime errors.

### 2026-07-17 resource lifetime audit

- Released temporary CSV/JSON buffers after the synchronous engine parsers
  consume them.
- Closed the hook, automatic-login, and manual read-pump thread handles after
  successful creation.
- Released the temporary command-line copy created by `CreateCmdLine`.
- Win32 Release build succeeded: `Release/CSOLauncher.exe`.
