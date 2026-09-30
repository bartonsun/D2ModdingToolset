# Custom lobby: saves and restart

Manual lobby/game IDs `+1..+7` and relay ID 255 are unchanged. New packet IDs are
relative to `ID_USER_PACKET_ENUM`; integers use the existing SLikeNet encoding.
Only the authenticated lobby can request a save or restart.

Ranked saves use the original host's native save builder, currently verified for
Russobit. Loaded games are casual. A room without the `Ranked` property is casual.
After login, a client advertises ranked lifecycle capability=1; older servers ignore
it, and current servers send lifecycle commands only to supporting clients.

| ID | Direction | Payload after message ID |
| --- | --- | --- |
| +8 SAVE_REQUEST | core → host | u64 saveId, u8 mode, ASCII save stem to packet end |
| +9 SAVE_UPLOAD | host → core | u64 saveId, u8 operation; BEGIN adds u32 totalSize, CHUNK raw bytes, COMMIT nothing, FAIL u8 result |
| +10 MATCH_ENDED | core → participant | Empty |
| +11 PLAYER_SETUP | participant → core | Kind 0: u32 capability=1, 16-byte install ID, u16 Windows major/minor, u32 build, u32 featureBits. Kind 1: i32 host lord category |
| +12 SYSTEM_NOTICE | core → participant | UTF-8 modal text to packet end; ordinary chat stays on legacy packets |
| +13 SAVE_STORED_ACK | core → host | u64 saveId |
| +14 SAVE_NATIVE_RESULT | host → core | u64 saveId, u8 result, successful filename to packet end; no filename on failure |
| +15 RESTART | core ↔ client | u8 operation, u64 token; 10 bytes including message ID |

Modes: Upload=0, LocalOnly=1. Operations: BEGIN=0, CHUNK=1, COMMIT=2, FAIL=3.
Results: Success=0, Failed=1, TimedOut=2. Save limits: 32 MiB, 30 seconds,
16 KiB per chunk; stems accept only ASCII letters, digits, `_` and `-`.
Capability=1 identifies this schema; saves do not repeat a version per packet.

The client chooses a process-unique filename and opens the result after GameSaved.
Upload keeps that exact file open until the durable server ACK permits deletion.
COMMIT contains no client digest: the server computes SHA-256 while receiving data.
LocalOnly reports the native result and keeps the file. Upload reports native success
through +14 and upload completion/failure through +9. Failures retain local saves.

## `111`

The host regenerates through the usual preview while guests wait. Room membership
and lobby connection remain; setup menus are skipped. Settings, races, lord/portrait,
difficulty and resources are restored. Lua getContents runs again for every preview;
accepted source bytes are retained, external Lua resources are not archived.

Only maps generated in this process have the complete recipe. A loaded `.sg` does
not reconstruct spins. All players need restart support (`featureBits & 1`);
older HELLO without featureBits means unsupported. Cleanup, settings restoration and
native readiness are separate barriers: host first, then guests. Cancel/disconnect
returns everyone to the lobby. A restart is not a match result and takes no final save.

Prepared match and online FilesHash extensions `+16/+17` are documented in
[PREPARED_MATCHES.md](PREPARED_MATCHES.md).
