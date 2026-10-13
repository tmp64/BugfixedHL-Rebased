# Scoreboard and HUD extension protocol

Status: Draft 3, not released. Protocol version 1. Covers the BugfixedHL client and server,
`bugfixedapi_amxx` and the GunGame plugin.

Draft 3 adds the element kinds `Board`, `Feed` and `Box`, the `Event` message, color slots, effects
common to every kind, the ring form of a `Meter`, the track of a `Strip` and sprite files as icons.
`HudV1` is not released, so all of it belongs to `HudV1` and needs no new caps bit (3.1).

This document describes how a BugfixedHL server sends to BugfixedHL clients a custom scoreboard
layout, extra values for players, teams and the whole game, and HUD elements defined by server
plugins. The client knows no game mode: a mode such as GunGame builds its scoreboard and HUD from
generic parts through plugin natives. The first consumer is the GunGame plugin (9.3).

## 1. Goals

- A server plugin can add columns to the scoreboard, hide some standard columns, move the others
  and fill cells for players, teams and the whole game.
- A server plugin can put HUD elements anywhere on the screen: text lines, icons, meters, icon
  strips, boards of rows, feeds of event lines and backdrops behind a group of elements. It
  chooses the position, alignment, color and font of each element, when it is shown, and its
  texts. Texts show cell values and places computed by the client.
- Texts are client localization tokens or the plugin's own texts, sent to every player in his
  language.
- The client contains no logic of a game mode.
- Full backward compatibility in both directions:
  - old clients (older BugfixedHL, Valve, AG, OpenAG) on a new server see the standard scoreboard
    and HUD and never receive the new message;
  - new clients on an old server (older BugfixedHL, AG, Valve) see the standard scoreboard and HUD.
- The protocol can grow without breaking clients that only know version 1.

Out of scope: demo playback, HLTV, AG and Valve server DLLs.

The data model is the Source engine `player_resource` idea, but it is not a networked entity.
`entity_state_t` has a fixed set of numeric fields and no strings, an entity reaches a client only
when it has a model and is in the client's PVS, and it can't be addressed to one client.
User messages carry strings, are reliable and can be sent to a single client.

## 2. Overview

```
plugin (AMXX) ── natives ──▶ bugfixedapi_amxx ── IBugfixedServer ──▶ hl.so: scoreboard and HUD state
                                                                          │
                                              "BhlScore" user message, MSG_ONE,
                                              only to clients with ScoreboardV1
                                                                          ▼
                                     client.dll: store ──▶ ScorePanel, hud_scores,
                                                           HUD elements (HudServerPanel)
```

The server keeps one state for everybody plus per-player overrides of texts and visibility, and
sends every client its own view of it. The client evaluates texts every frame from the store, for
the player it looks at. Events are the exception: one-shot lines that the server queues per client
and the client evaluates once, on arrival (4.3, 5.6).

| Term       | Meaning                                                                                    |
|------------|--------------------------------------------------------------------------------------------|
| Schema     | `Config` plus the list of columns                                                          |
| Target     | Owner of a value: the whole game, a player or a team                                       |
| Cell       | One value, addressed by target and column                                                  |
| List       | A named array of values that belongs to the whole game                                     |
| Element    | A HUD item defined by the server: `Text`, `Icon`, `Meter`, `Strip`, `Board`, `Feed`, `Box` |
| Template   | Text of an element with placeholders, evaluated by the client (8.10)                       |
| Variant    | One of up to 8 templates of an element, chosen by a value                                  |
| Color slot | A color of an element for a variant, the warning or the accent bar (8.5)                   |
| Event      | A line for a `Feed` element, sent once and never kept as state (4.3)                       |
| Viewer     | The client that draws the HUD                                                              |
| Subject    | The player whose data an element shows: the viewer or the observed player                  |
| Caps       | Bitmask of client features, reported by the client in `_bhl`                               |

## 3. Client capabilities

### 3.1 Format

The client reports its features in the cvar and userinfo key `_bhl`. The value is a bitmask
written as lowercase hex without a prefix, at most 8 digits: `1`, `3`, `1f`.

The server accepts only 1–8 hex digits of either case, without a sign, spaces or `0x`, and parses
them with `strtoul(value, nullptr, 16)`. Any other value, an empty one included, means `0`. Unknown
bits are ignored.

| Bit  | Mask  | Name           | Meaning                                                         |
|------|-------|----------------|-----------------------------------------------------------------|
| 0    | `0x1` | `ScoreboardV1` | Understands `BhlScore` protocol version 1 and receives its data |
| 1    | `0x2` | `HudV1`        | Draws HUD elements; the plugin drops its own HUD texts          |
| 2–31 | —     | reserved       | Clients send 0, servers ignore                                  |

`HudV1` is only set together with `ScoreboardV1`, because elements read the store. The server
doesn't look at `HudV1` when it sends: every client with `ScoreboardV1` gets the HUD data too. The
bit tells plugins which players need their own HUD texts. The server clears `HudV1` when
`ScoreboardV1` is not set or the client is a bot or an HLTV proxy, so a plugin never drops its texts
for a client that gets no data. `bhl_get_client_caps()` returns this normalized value.

Bits are never reused or redefined after release. An incompatible protocol change gets a new bit
(`ScoreboardV2`), and the server sends the new format only to clients that have that bit. Before
release, bit 1 and `Config` changed meaning; draft 1 builds are not compatible (11). Draft 3
extends `HudV1` before its release (new kinds, params, color slots, the `Event` op, built-in
values and sprite files) without a new bit; draft 2 builds are not supported either (11).

`_bhl` doesn't replace `aghl_supports`. The MOTD flags stay where they are.

### 3.2 Client side

- `ConVar _bhl` has only `FCVAR_USERINFO`.
  - No `FCVAR_ARCHIVE` or `FCVAR_BHL_ARCHIVE`: the value is computed, it must not be saved to configs.
  - No `FCVAR_PRIVILEGED`: the engine answers a cvar query for such cvars with `CVAR is privileged`,
    which would break the server fallback from 3.3.
- The cvar is registered with the caps of the build as its default value (`3`), so the key is in
  userinfo from the first connect.
- One owner function, `CHud::UpdateCapsUserinfo()`, computes the caps from the build
  features and client settings and writes `_bhl`. It runs:
  - on the first frame after configs have been executed (`m_NextFrameQueue`);
  - when a related client setting changes;
  - on every `VidInit`.

  A value typed by the user is overwritten by the next call.
- One frame after a write the client compares `gEngfuncs.LocalPlayerInfo_ValueForKey("_bhl")` with
  the expected value:
  - on mismatch it runs `setinfo _bhl <caps>`;
  - if the key is still wrong on the next check, it prints one console message (most likely the
    userinfo is full). The server can still detect the client with the cvar query from 3.3.
- Client settings:

  | Cvar                    | Default | Flags               | Meaning                                              |
  |-------------------------|---------|---------------------|------------------------------------------------------|
  | `hud_scoreboard_custom` | 1       | `FCVAR_ARCHIVE`     | Show the custom scoreboard when the server sends one |
  | `hud_server_elements`   | 1       | `FCVAR_BHL_ARCHIVE` | Draw HUD elements defined by the server              |

  - `ScoreboardV1` is set while either cvar is 1, `HudV1` while `hud_server_elements` is 1.
  - `hud_scoreboard_custom 0` alone: the client still receives the data and draws elements, and
    shows the standard scoreboard.
  - `hud_server_elements 0` alone: `HudV1` is cleared. The client draws no screen elements (the
    banner rows stay, 6.8) and ignores requests to hide standard HUD parts (8.12). The plugin shows
    its own HUD texts again. Nothing is sent again.
  - Both 0: both bits are cleared and the server stops sending. When the client clears
    `ScoreboardV1` in `_bhl`, it clears its store as on `Reset`. Setting either cvar back to 1
    brings a full sync.
  - While `ScoreboardV1` is cleared, the client ignores `BhlScore`. After it sets the bit again, it
    ignores every message except `Reset` until a `Reset` or `VidInit`, so changes meant for the old
    store are never applied to the empty one.

### 3.3 Server side

For every client the server keeps `caps` and the source they came from:

| Source     | Set when                                                      |
|------------|---------------------------------------------------------------|
| `Unknown`  | On connect, before anything is known                          |
| `Userinfo` | `_bhl` is present in userinfo                                 |
| `Query`    | The cvar query answered; errors give `caps = 0`               |
| `None`     | No key, and a query is impossible: bot, HLTV, p47, LAN client |

1. `ClientConnect` and `ClientUserInfoChanged` read `_bhl` from userinfo. If the key is present,
   it is parsed and the source becomes `Userinfo`. Userinfo always wins: a query answer that
   arrives later doesn't overwrite it. If the key disappears from userinfo, the source goes back
   to `Unknown` and the caps stay as they are until the query of step 2 gives new ones; a client
   past `InitHUD` is queried at once.
2. In `InitHUD`, if the source is `Unknown` and either the extension is active (5.1) or
   `sv_bhl_query_vars` is 1, the server calls `QueryClientCvarValue2(_bhl)` with a new
   `REQUESTID_CAPS`. It applies the same p47 and LAN checks as the existing `aghl_*` queries and
   waits while the SteamID is pending. If the extension becomes active later on the map, clients
   that are still `Unknown` are queried at that moment.
   While the source is `Query` and the extension is active, the server queries `_bhl` again every
   10 s, so a changed client setting reaches it.
3. An answer that doesn't parse (`Bad CVAR request`, `CVAR is privileged`, empty) means `0`.
4. Every change of `caps` fires the plugin forward `bhl_client_caps_changed(id)`.
   - `ScoreboardV1` appeared after `InitHUD`: the client gets a full sync (5.3).
   - `ScoreboardV1` disappeared: the server stops sending to that client and doesn't send `Reset`.
   - Only `HudV1` changed: nothing is sent; the forward lets the plugin switch its own HUD texts.

Each layer covers a different case:

| Layer       | Covers                                                              |
|-------------|---------------------------------------------------------------------|
| Cvar mirror | The normal case: no extra traffic, known before `InitHUD`           |
| `setinfo`   | The cvar was not mirrored into userinfo for any reason              |
| Cvar query  | Userinfo is full (256 bytes), so neither path above can add the key |

The server command `bhl_caps` prints slot, name, caps and source for every connected client.

Known limitation: with the source `Query` the server sees a changed setting only with the next
answer, up to 10 s later. A client that switches both cvars off and back on between two answers
clears its store and waits for a `Reset` (3.2), but the server never saw `ScoreboardV1` disappear
and sends only changes. The client shows the standard scoreboard and no elements until the next
map or a reconnect.

## 4. Message `BhlScore`

### 4.1 Transport

- The server registers `REG_USER_MSG("BhlScore", -1)`: variable size, payload up to 192 bytes.
- The message is sent only with `MSG_ONE` (reliable) and only to clients that have
  `ScoreboardV1` and passed `InitHUD`. It is never sent with `MSG_ALL`, `MSG_BROADCAST` or
  `MSG_ONE_UNRELIABLE`, and never to bots or HLTV proxies.
  - Old clients never receive it, so it doesn't matter how their engine handles unknown messages.
  - An old server never registers it, so the client hook never fires.
  - Every message is built for one client, so per-player texts and visibility cost no extra
    format: the server sends each client its own values.
- One message carries one operation. The first byte is the opcode, the rest is its payload.
  The client ignores messages with unknown opcodes, which lets new opcodes be added safely.
- `Reset`, `Config`, `Column`, `Element`, `Template` and `HudState` are read field by field, and
  trailing bytes after the known fields are ignored. Later versions may append fields to them, as
  draft 3 appends the color block to `Element`. `Values`, `List` and `Event` have no room for
  trailing fields and are extended through new value types or new opcodes.

### 4.2 Data types

All integers are little-endian, as written by `WRITE_*` and read by `READ_*`.

| Type     | Bytes | Write / read                   | Range                 |
|----------|-------|--------------------------------|-----------------------|
| `byte`   | 1     | `WRITE_BYTE` / `READ_BYTE`     | 0 … 255               |
| `short`  | 2     | `WRITE_SHORT` / `READ_SHORT`   | −32768 … 32767        |
| `word`   | 2     | `WRITE_SHORT` / `READ_WORD`    | 0 … 65535             |
| `long`   | 4     | `WRITE_LONG` / `READ_LONG`     | −2³¹ … 2³¹−1          |
| `string` | n + 1 | `WRITE_STRING` / `READ_STRING` | UTF-8, NUL-terminated |

Strings:

- A client token is `#` followed by `[A-Za-z_][A-Za-z0-9_]*`. A Column title, the Config title and
  tag, and the text of a `Text` or `Icon` value (in cells and list items) that is exactly a token
  is resolved by VGUI2 on the client. If the token is missing, the client shows it without the
  leading `#`. Any other string is shown as it is: `#1` stays `#1`. Templates follow 8.10.
- Plugins can only use tokens shipped with the client in
  `ui/resource/language/bugfixedhl_*.utf8.txt`. The client ships generic tokens for columns and HUD
  texts, see section 10.
- Color codes (`^` and a digit) are not interpreted in version 1. The client strips them from every
  string except `select`, `value` and `list` of an `Element` and the text of a `Template`, where `^`
  starts a ref (8.10); the template parser drops them from literals and word forms.
- The server truncates strings to the limits from 4.7 on a UTF-8 character boundary; templates are
  never cut (9.2). The client truncates strings again if they are still too long, on a UTF-8
  character boundary.

### 4.3 Opcodes

| Op     | Name       | Payload                                        |
|--------|------------|------------------------------------------------|
| `0x00` | `Reset`    | Protocol version                               |
| `0x01` | `Config`   | Hidden standard columns, title, tag            |
| `0x02` | `Column`   | One column definition                          |
| `0x03` | `Values`   | Cells for one or more targets                  |
| `0x04` | `List`     | Items of one list                              |
| `0x05` | `Element`  | One HUD element definition, or a removal       |
| `0x06` | `Template` | One template variant of an element             |
| `0x07` | `HudState` | Visible elements and hidden standard HUD parts |
| `0x08` | `Event`    | One line of a `Feed` element; not state        |
| other  | reserved   | Ignored by the client                          |

#### `Reset` (`0x00`)

| Field     | Type   | Notes                 |
|-----------|--------|-----------------------|
| `version` | `byte` | Protocol version, `1` |

The client drops the schema, all cells, lists, elements and templates, sets the HUD state to "no
element visible, no standard part hidden" and returns to the standard scoreboard. `version` is
informational: the client reads the fields it knows. A version 1 client accepts any `version ≥ 1`
and ignores a `Reset` with `version 0`.

#### `Config` (`0x01`)

| Field   | Type     | Notes                                                |
|---------|----------|------------------------------------------------------|
| `flags` | `word`   | See below                                            |
| `title` | `string` | Up to 63 bytes; replaces the server name if not `""` |
| `tag`   | `string` | Up to 15 bytes; a badge after the server name: `FFA` |

| Bit  | Name          | Meaning                 |
|------|---------------|-------------------------|
| 0    | `HideFrags`   | Hide the Frags column   |
| 1    | `HideDeaths`  | Hide the Deaths column  |
| 2    | `HideEff`     | Hide the Eff column     |
| 3    | `HideSteamID` | Hide the SteamID column |
| 4–15 | reserved      | Sent as 0, ignored      |

The server can't hide Avatar, Name and Ping. Avatars follow the player's
`hud_scoreboard_showavatars`.

The first `Config` after a `Reset` (or after `VidInit`) turns the custom scoreboard on.
A repeated `Config` replaces the previous one. HUD elements don't need a `Config`: a plugin that
only wants HUD elements leaves the standard scoreboard in place. A `Config` without a whole `flags`
is ignored; a missing `title` or `tag` is `""`.

#### `Column` (`0x02`)

| Field     | Type     | Notes                                                                            |
|-----------|----------|----------------------------------------------------------------------------------|
| `id`      | `byte`   | 0–31                                                                             |
| `display` | `byte`   | How cells are drawn, see below                                                   |
| `flags`   | `word`   | See below                                                                        |
| `width`   | `byte`   | Proportional units as in `ScorePanel.res`; `0` is the default                    |
| `sort`    | `byte`   | `0`: not a sort key; bits 0–6: priority, 1 first; bit 7: ascending               |
| `key`     | `string` | Up to 15 bytes of `[a-z0-9_]`, unique: `level`, `weapon`                         |
| `title`   | `string` | Up to 31 bytes, text or `#token`; `""` for a built-in column keeps its own title |
| `host`    | `byte`   | `Badge` only: id of the column the badge follows; `0xFF` otherwise or if unknown |

A `Column` with an existing `id` replaces that column's definition; its cells are kept.
Columns are shown in ascending `id` order. The server assigns ids, plugins address columns
by `key`. Templates and elements also refer to columns by `key` (8.10), so they don't depend on ids.
The server keeps `host` as a key and resolves it when it builds the message (5.1).

The client ignores a `Column` with an `id` of 32 or more, and one that ends before `key` or whose
`key` has no NUL. A missing `title` is `""`, a missing `host` is `0xFF`. `Builtin` and `Badge`
columns are never sort keys; keys with the same priority are ordered by `id`.

Display kinds:

| Value | Name      | Draws                                                                              |
|-------|-----------|------------------------------------------------------------------------------------|
| 0     | `Text`    | Any cell as text                                                                   |
| 1     | `Icon`    | `Icon` cells as sprite and text; other cells as text                               |
| 2     | `Meter`   | `Progress` cells as dots or a bar with a `cur/max` counter (6.6); others as text   |
| 3     | `Builtin` | A standard column, chosen by `key`; cells are ignored                              |
| 4     | `Steps`   | `Progress` cells as `cur/max` with a strip of `max` steps below (6.6)              |
| 5     | `Badge`   | Not a column of its own: cells are drawn as badges after the cells of `host` (6.7) |
| other | —         | Same as `Text`                                                                     |

Default widths for `width = 0`: `Text` 60, `Icon` 100, `Meter` 90, `Steps` 60. A `Builtin` column
keeps the width of its standard column. A `Badge` column takes no width.

Built-in columns:

| `key`     | Column  | Notes                                                   |
|-----------|---------|---------------------------------------------------------|
| `rank`    | `#`     | Place, computed by the client (6.4); always drawn first |
| `frags`   | Frags   |                                                         |
| `deaths`  | Deaths  |                                                         |
| `eff`     | Eff     | Also follows `hud_scoreboard_showeff`                   |
| `steamid` | SteamID | Also follows `hud_scoreboard_showsteamid`               |
| `ping`    | Ping    |                                                         |
| other     | —       | The client ignores the column; the server accepts it    |

A `Builtin` column moves a standard column to its own position. Standard columns that the schema
doesn't mention keep their default places. `Hide*` flags win over a `Builtin` column. The client
ignores a `Hidden` `Builtin` column: the standard column keeps its default place, and a hidden
`rank` gives no place column.

Column flags:

| Bits | Name       | Meaning                                                      |
|------|------------|--------------------------------------------------------------|
| 0    | `Hidden`   | Data only: not drawn, still used for sorting and by elements |
| 1–2  | `Align`    | 0 left, 1 center, 2 right; 3 is treated as left              |
| 3    | `Primary`  | The main score: `hud_scores` shows it instead of frags       |
| 4    | `MarkLast` | `Steps`: the last step has its own color (6.6)               |
| 5–15 | reserved   | Sent as 0, ignored                                           |

A `Hidden` column can carry values for the whole game only, for example a game phase. The server
sends `sort = 0` and clears `Primary` for a `Badge` column. `Hidden` on a `Badge` column hides its
badges.

#### `Values` (`0x03`)

The payload is a sequence of groups that runs to the end of the message:

```
Values := op:byte(0x03) Group+
Group  := target:byte count:byte Cell{count}
Cell   := column:byte Value
Value  := type:byte len:byte payload:byte{len}
```

Targets:

| Value         | Target                                               |
|---------------|------------------------------------------------------|
| `0x00`        | The whole game                                       |
| `0x01`–`0x40` | Player 1–64                                          |
| `0x81`–`0xC0` | Team 1–64, numbered like `teamnumber` in `ScoreInfo` |
| other         | Reserved: the client skips the group                 |

Cell fields:

- `column` is the column id. `0xFF` together with type `None` clears all cells of the target.
- `type` is the value type, see below.
- `len` is the payload length. The client always moves on by `len` bytes, so it can skip
  types it doesn't know.

Value types:

| Value | Name       | `len`   | Payload                                                |
|-------|------------|---------|--------------------------------------------------------|
| 0     | `None`     | 0       | Nothing: clears the cell                               |
| 1     | `Int`      | 1, 2, 4 | Signed integer, `len` bytes                            |
| 2     | `Text`     | 1–32    | `string`                                               |
| 3     | `Icon`     | 2–64    | `string sprite`, `string text`                         |
| 4     | `Progress` | 2 or 4  | `cur`, `max`: two `byte` for `len` 2, two `word` for 4 |
| 5     | `Clock`    | 5       | `long ms`, `byte flags`: bit 0 runs, bit 1 counts down |
| other | —          | any     | Unknown type: the cell is cleared                      |

- A known type with a wrong `len` or a broken payload clears the cell.
- If the message ends in the middle of a group or a cell, the client keeps the cells it has
  already applied and drops the rest.
- Cells of columns that don't exist yet are stored and shown when the column arrives.
- `Icon.sprite` is a sprite name from `hud.txt`, the same names the kill feed uses (`d_9mmAR`),
  or the path of a sprite file: `sprites/<name>.spr`, at most 31 bytes of `[A-Za-z0-9_./-]`
  without `..` (`IsSpriteFilePath`). The server sends both as they are; a plugin precaches its file
  with `precache_generic` in `plugin_precache`, so clients download it. The client loads a file on
  first use and draws its first frame whole (6.6). If the client doesn't have the sprite, it draws
  only `text`. For standard HL weapon sprites the client shows its own weapon name instead of
  `text` (10.3).
- `Progress` with `max = 0` is drawn as `cur`.
- `Clock` is a time the client runs itself, for timers that change every frame. On arrival the
  client stores its own time `t0`. A stopped clock is `ms`; a running one is `ms + (now − t0)`, or
  `ms − (now − t0)` when it counts down, and stops at 0. Its number is the current value in whole
  seconds, rounded up when it has the "counts down" flag (running or stopped) and down otherwise;
  `time2` (8.10) shows the milliseconds.

Sort keys: `Int` uses the value, `Progress` uses `cur`, `Clock` its number, `Text` and `Icon`
compare text case-insensitively. Empty cells go after filled ones.

#### `List` (`0x04`)

A list is a named array of values that belongs to the whole game, for example the weapons of all
GunGame levels.

```
List := op:byte(0x04) id:byte total:byte start:byte key:string Value*
```

| Field   | Type     | Notes                                                    |
|---------|----------|----------------------------------------------------------|
| `id`    | `byte`   | 0–7, assigned by the server                              |
| `total` | `byte`   | Number of items, 0–64; the client resizes the list to it |
| `start` | `byte`   | Index of the first item in this message                  |
| `key`   | `string` | Up to 15 bytes of `[a-z0-9_]`, unique: `levels`          |
| items   | `Value*` | Items `start`, `start + 1`, … up to the end of message   |

- A long list is split into several messages with different `start`.
- Items at `total` or beyond are dropped. An item of an unknown type is stored as empty.
- `total = 0` removes the list.
- Items added by a resize are always sent, as `None` if they were not set.

#### `Element` (`0x05`)

```
Element   := op:byte(0x05) id:byte kind:byte
             flags:word x:short y:short align:byte place:byte parent:byte gap:byte
             color:byte{4} font:byte select:string value:string list:string
             count:byte Param{count} [colors:byte ColorSlot{colors}]
Param     := id:byte value:short
ColorSlot := slot:byte r:byte g:byte b:byte a:byte
```

| Field       | Type           | Notes                                                                                                      |
|-------------|----------------|------------------------------------------------------------------------------------------------------------|
| `id`        | `byte`         | 0–31, assigned by the server. `0xFF` with kind `None` removes every element                                |
| `kind`      | `byte`         | See below; `None` and unknown values remove the element                                                    |
| `flags`     | `word`         | See below                                                                                                  |
| `x`         | `short`        | Horizontal position in 1/8192 of the screen width; `-8192` (−1.0) is the center (8.4)                      |
| `y`         | `short`        | Vertical position, the same way                                                                            |
| `align`     | `byte`         | Anchors: bits 0–1 horizontal, bits 2–3 vertical, see below                                                 |
| `place`     | `byte`         | What the position is relative to, see below                                                                |
| `parent`    | `byte`         | Element id for the parent places; `0xFF` otherwise or if the parent doesn't exist                          |
| `gap`       | `byte`         | Proportional units: distance to the parent, the margin on the numbers row (8.4), the padding of a `Box`    |
| `color`     | `byte` × 4     | r, g, b, a. With `HudColor` only `a` is used                                                               |
| `font`      | `byte`         | Font id (8.5); unknown ids mean 0                                                                          |
| `select`    | `string`       | A ref whose number picks the `Text` variant (8.10), `""` for variant 0, or the own row of a list `Board`   |
| `value`     | `string`       | The bound ref: `{@value}`, the progress of a `Meter`, the icon of an `Icon`, the position of a `Strip`, …  |
| `list`      | `string`       | `Strip`: key of the list with the icons; `Board`: key of the list of rows; `""` otherwise                  |
| `count`     | `byte`         | Number of params, 0–32                                                                                     |
| `Param`     | `byte` `short` | Param id and value (8.7–8.9, 8.13–8.16); unknown ids and params of other kinds are ignored, values clamped |
| `colors`    | `byte`         | Optional: number of color slots, 0–10; sent only when the element has a slot color                         |
| `ColorSlot` | `byte` × 5     | Slot 0–9 and r, g, b, a (8.5); the client skips slots of 10 and above                                      |

Kinds:

| Value | Name    | Draws                                                              |
|-------|---------|--------------------------------------------------------------------|
| 0     | `None`  | Nothing: removes the element                                       |
| 1     | `Text`  | One line of text (8.6)                                             |
| 2     | `Meter` | Dots, a bar or a ring of dots with a counter (8.7)                 |
| 3     | `Strip` | A row of cells with icons, optionally with the other players (8.8) |
| 4     | `Icon`  | A sprite with a text after it (8.9)                                |
| 5     | `Board` | Rows of the scoreboard order or of a list (8.14)                   |
| 6     | `Feed`  | Lines of `Event` messages (8.15)                                   |
| 7     | `Box`   | A backdrop behind the group of its parent (8.16)                   |
| other | —       | Removes the element                                                |

- hl.so converts a screen fraction `f` from the plugin to `(int)floor(f · 8192 + 0.5)`, clamped to
  −32768…32767. `x = −8192` exactly is the center. This is the fixed point of `TE_TEXTMESSAGE`,
  which `set_hudmessage` also uses, but rounded instead of truncated: 0.025 is 205, not 204.
- A ref in `select` and `value` is a column key or a built-in value such as `@state` (8.11),
  optionally with the prefix `^` and one of the formats `cur`, `max` and `left` (8.10):
  `level:left`. The longest valid ref has 21 bytes: `^`, a key of 15 bytes and `:left`. With a
  format the value is an `Int`: a `Meter` or an `Icon` bound to it has no content, a `Strip` uses
  it as the position. `list` is a list key.
- The server keeps `parent` as a key and resolves it when it builds the message (5.1).
- With kind `None` the message ends after `kind`.

Anchors:

| Bits | Anchor     | 0    | 1    | 2      | 3      |
|------|------------|------|------|--------|--------|
| 0–1  | Horizontal | auto | left | center | right  |
| 2–3  | Vertical   | auto | top  | middle | bottom |

Places:

| Value | Name           | Position                                                                |
|-------|----------------|-------------------------------------------------------------------------|
| 0     | `Screen`       | `x`, `y` and the anchors                                                |
| 1     | `Parent`       | Below or above element `parent`; `x` and the horizontal anchor as usual |
| 2     | `NumbersRow`   | On the row of the HL health, armor and ammo numbers; `x` as usual       |
| 3     | `BannerTop`    | Scoreboard banner, on the server name row (6.8)                         |
| 4     | `BannerBottom` | Scoreboard banner, on the map name row (6.8)                            |
| 5     | `ParentLeft`   | Left of element `parent`, centered vertically on it                     |
| 6     | `ParentRight`  | Right of element `parent`, centered vertically on it                    |
| other | —              | Same as `Screen`                                                        |

`Parent`, `ParentLeft` and `ParentRight` are the parent places (8.4).

Flags:

| Bit  | Name           | Meaning                                                                              |
|------|----------------|--------------------------------------------------------------------------------------|
| 0    | `HudColor`     | rgb follows `hud_color`; on a banner row, the scheme color of the row (6.8)          |
| 1    | `Subject`      | Needs a subject (8.2): hidden when there is none                                     |
| 2    | `NoScoreboard` | Hidden while the scoreboard is open                                                  |
| 3    | `Leader`       | The subject is the first row of the scoreboard order instead of the viewer's subject |
| 4–15 | reserved       | Sent as 0, ignored                                                                   |

- The client ignores an `Element` with an id of 32 or more (except `0xFF` with `None`), and one
  that ends before `font`. A missing string, or one without a NUL, means `""`. All `count` params
  are read while at least 3 bytes remain, unknown ids and params of other kinds included, so the
  color block after them is found; missing params take their defaults. A missing `colors` means 0,
  and slots are read while at least 5 bytes remain.
- The server writes params in ascending id order. Params added in draft 3 have ids from 32 on, so
  they follow all older ones, and the color block is written only when at least one slot is set:
  an element that uses nothing of draft 3 is byte-identical to draft 2.
- A definition for an existing id replaces it. An `Element` never changes templates: the client
  drops the templates of an id only on a removal (kind `None` or an unknown kind, for that id or
  `0xFF`) and on `Reset`. When the server drops the texts of an element, it sends a removal right
  before the new definition (5.2).
- The largest message is a `Strip`, the kind with the most params, with every param and every
  color slot: 18 fixed bytes, strings of 22, 22 and 16 bytes, 1 + 17 × 3 bytes of params and
  1 + 10 × 5 bytes of colors, 181 bytes. The server still refuses a change that would make an
  `Element` longer than 192 bytes (9.2).

#### `Template` (`0x06`)

```
Template := op:byte(0x06) id:byte variant:byte text:string
```

| Field     | Type     | Notes                                                      |
|-----------|----------|------------------------------------------------------------|
| `id`      | `byte`   | Element id                                                 |
| `variant` | `byte`   | 0–7                                                        |
| `text`    | `string` | Template (8.10), up to 188 bytes; `""` removes the variant |

- The client ignores a `Template` for an undefined element, a variant of 8 or more, and a text
  without a NUL. The server always sends `Element` first.
- The server sends the text the plugin set for this player, or the default text (5.4). The client
  doesn't know which one it got.
- The largest message is 192 bytes.

#### `HudState` (`0x07`)

```
HudState := op:byte(0x07) visible:long hide:word
```

| Field     | Type   | Notes                                                  |
|-----------|--------|--------------------------------------------------------|
| `visible` | `long` | Bit per element id: the plugin shows the element (8.3) |
| `hide`    | `word` | Standard HUD parts to hide (8.12)                      |

The message is absolute: it replaces the previous state. After `Reset` and `VidInit` both fields
are 0. The server sends `HudState` after element definitions and templates (5.2), so elements
never appear before their texts.

- `visible` is an unsigned 32-bit mask. Bits of undefined ids are stored and apply when the
  element arrives.
- A `HudState` shorter than 5 bytes is ignored; a missing `hide` means 0.

#### `Event` (`0x08`)

```
Event := op:byte(0x08) id:byte variant:byte life:word count:byte Value{count}
```

| Field     | Type    | Notes                                                                   |
|-----------|---------|-------------------------------------------------------------------------|
| `id`      | `byte`  | Element id of a `Feed`                                                  |
| `variant` | `byte`  | Template variant of the line, 0–7                                       |
| `life`    | `word`  | How long the line is shown, ms; 0 is the `life_ms` param of the element |
| `count`   | `byte`  | Number of args; the client reads at most 5                              |
| `Value`   | `Value` | Args in the encoding of `Values`: `type`, `len`, payload                |

- An event is not state. The client ignores it unless element `id` exists, is a `Feed` and has a
  template for `variant`. It evaluates that template once, on arrival, for the subject of the
  element (8.2) with the args as `@a`–`@d` and `@icon` (8.11), and keeps the result as a line
  stamped with its own time (8.15).
- The first arg of type `Icon` is also the icon of the line. An arg of an unknown type, with a wrong
  `len` or a broken payload keeps its position as an empty value. An `Event` with a short header,
  or one that ends before its first `min(count, 5)` args are read, is ignored.
- A line whose template evaluates to nothing (a missing value, 8.10) and that has no icon sprite is
  dropped.
- Lines change no serial and are never replayed by a full sync. They are dropped with their element
  (a removal, `Reset`, `VidInit`, the loss of `ScoreboardV1`). The client keeps 32 lines over all
  elements; a new line replaces the oldest one.
- An event takes 6 bytes plus `2 + len` per arg. The natives send at most 4 `Int` args and an
  `Icon`, at most 96 bytes. The server sends events last (5.2, 5.6).

### 4.4 Example: one GunGame row

Player 5 is on level 12 of 24 with the MP5 and has 1 of 3 kills. Column ids: `level` 1,
`weapon` 2, `progress` 3.

```
03                                         op Values
05 03                                      target: player 5, 3 cells
01 04 02  0c 18                            level:    Progress, len 2, cur 12, max 24
02 03 0c  64 5f 39 6d 6d 41 52 00          weapon:   Icon, len 12, sprite "d_9mmAR"
          4d 50 35 00                                text "MP5"
03 04 02  01 03                            progress: Progress, len 2, cur 1, max 3
```

The group takes 27 bytes. With the `kills` and hidden `score` cells (8–10 more bytes) a player
takes 35–37 bytes, so one message holds 5 players and a full sync of 32 players needs 7 `Values`
messages.

### 4.5 Example: one element

The place line of GunGame: a `Text` element at the top center, large font, `hud_color`. Its
variant is chosen by the `phase` cell, and `{@value}` in its templates is the `phase_left` cell.

```
05                                  op Element
00 01                               id 0, kind Text
07 00                               flags: HudColor, Subject, NoScoreboard
00 e0                               x = -8192: center
cd 00                               y = 205: 0.025 · 8192 = 204.8, rounded
00 00                               align: auto anchors; place: Screen
ff 00                               no parent, gap 0
00 00 00 ff                         color: rgb ignored (HudColor), alpha 255
01                                  font: HudServerLarge
70 68 61 73 65 00                   select "phase"
70 68 61 73 65 5f 6c 65 66 74 00    value "phase_left"
00                                  list ""
00                                  no params
```

The message takes 37 bytes. GunGame also sets four params of 8.13 on this element (9.3), so its
`count` is 4 and 12 bytes of params follow. Variant 0 with a client token takes 18:

```
06 00 00                                       op Template, element 0, variant 0
23 42 48 4c 5f 48 55 44 5f 50 6c 61 63 65 00   "#BHL_HUD_Place"
```

### 4.6 Sizes for GunGame

| Message                                    | Bytes                         |
|--------------------------------------------|-------------------------------|
| `Reset`                                    | 2                             |
| `Config`                                   | ~8                            |
| `Column` × 9                               | ~14–34                        |
| `List` `levels`, 24 levels                 | 2 × ≤ 192                     |
| `Values`, 32 players                       | 7 × ≤ 192                     |
| `Element` × 10                             | 25–49, ~360 in all            |
| `Template` × 17, plugin texts in Russian   | 18–115, ~850 in all           |
| `HudState`                                 | 7                             |
| Level-up of one player                     | ~33, +17 when `final` changes |
| `Event` of a level change, to each `HudV1` | 6–~40                         |
| Kill without level-up                      | ~18                           |
| Warmup second                              | ~8                            |
| Warmup start or end                        | ~190                          |

At the start and the end of the warmup the GunGame plugin sends the `level`, `weapon`, `progress`,
`kills` and `score` columns again, changes the visible elements and the `flash_ms` of `gg_place`
(9.3). The banner needs no traffic until the win: its rows are chosen by the leader's `level` cell
(9.3).

### 4.7 Limits

| What                              | Limit                                          |
|-----------------------------------|------------------------------------------------|
| Message payload                   | 192 bytes                                      |
| Columns                           | 32 (ids 0–31)                                  |
| Lists                             | 8 (ids 0–7)                                    |
| List items                        | 64                                             |
| Column and list `key`             | 15 bytes                                       |
| Column title                      | 31 bytes                                       |
| Config title                      | 63 bytes                                       |
| Config tag                        | 15 bytes                                       |
| `Text` value                      | 31 bytes                                       |
| `Icon` sprite and text            | 31 bytes each                                  |
| Dots in a scoreboard `Meter`      | `max ≤ 10`                                     |
| Elements                          | 32 (ids 0–31)                                  |
| Element key (server only)         | 15 bytes of `[a-z0-9_]`                        |
| Ref in `select` and `value`       | 21 bytes (4.3)                                 |
| Params per element                | 32 (ids 0–63); at most 17 exist for one kind   |
| Color slots per element           | 10 (slots 0–9)                                 |
| Variants per element              | 8                                              |
| Template                          | 188 bytes                                      |
| Placeholders per template         | 16                                             |
| Evaluated text                    | 255 UTF-16 units                               |
| `Board` rows                      | 16 by `rows`, plus the separator and own row   |
| `Board` text columns              | 4                                              |
| `Feed` lines shown                | 8 by `lines`                                   |
| `Feed` lines kept by the client   | 32 over all elements                           |
| Args of an `Event`                | 5 read; the natives send 4 `Int` and an `Icon` |
| Events queued per client (server) | 8, the oldest is dropped                       |
| Sprite files per game (client)    | 32 different paths                             |

String limits are in bytes, without the terminating NUL.

## 5. Server behaviour

### 5.1 State

One object in `hl.so`, `CScoreboardExt`, holds the config, the columns, the lists, the cells of
the whole game, every player and every team, the HUD elements with their default templates and
visibility, the hidden standard HUD parts, and the per-player overrides of templates and
visibility (5.4). For each client it also keeps `caps`, their source, a `ready` flag (set after
`InitHUD`), pending `Reset`, header, elements, templates and HUD state, and sets of dirty cells
and list items. Apart from this sync state it keeps a queue of events for each client (5.6).

- Column masks have 32 bits, one per column id. The mask of all columns is built without
  `1u << 32`, which is undefined in C++. The params of an element have a 64-bit mask
  (`ParamMask`, ids 0–63), its color slots a mask of 10 bits.
- Element parents and badge hosts are stored as keys and resolved to ids when a message is built,
  `0xFF` when the key doesn't exist. Adding, removing or re-adding an element or a column marks
  pending every element or badge column that names it.

The extension is active from the first `Config`, element or hidden standard part until the plugin
resets it or the map changes. While it is inactive, a client gets only `Reset`.

All plugins share this one state. Columns, lists and elements of different plugins live side by
side when their keys differ, so a plugin prefixes its keys (`gg_`). `bhl_sb_reset()` clears the
whole state, of every plugin.

### 5.2 Sending

Plugins only change the state and mark the changes pending for every ready client. In
`StartFrame` the server walks the ready clients that have `ScoreboardV1` and sends:

1. `Reset`, if one is pending;
2. `Config`, if it is pending, and every pending `Column`;
3. the dirty list items packed into `List` messages;
4. the dirty cells packed into `Values` messages;
5. every pending `Element`: definitions and removals;
6. every pending `Template`, with the text for this client;
7. `HudState`, if it is pending, with the visibility for this client;
8. the queued events of this client, in order (5.6).

A client gets at most about 512 bytes per 0.1 s, so the rate stays about 5 KB/s at any server
fps. When they are used up, nothing more is sent to it until the 0.1 s is over. Small updates go
out in the frame they happen while the budget lasts. Several changes of one cell, element,
template or visibility in one frame produce one message. A setter that writes the value already
stored marks nothing, but a write for player 0 still drops the overrides (5.4).

A new element gets the lowest free id that no client still has anything of (the old element, its
visibility bit, a child placed at it); only when every free id is held is the lowest free id
reused. When the server drops the texts of an element (removal, kind change), a client that has
the element gets `Element` kind `None` for that id right before the new definition, both in one
frame or neither; then only non-empty variants are sent. A variant whose text is removed is sent as
`""`, and only to a client that holds that variant. Templates of an id with no element are not
sent. So a new element never shows old variants. A removal goes only to a client that has the
element, and `HudState` only to a client whose visibility or hidden parts differ from what it has.

### 5.3 Full sync

A full sync is `Reset`, the header, every list item, every non-empty cell, every element, every
template this client should have and `HudState`, marked pending for that client. Events are not
part of it: a full sync neither clears the event queue nor replays sent events (5.6). It starts:

- about 0.5 s after `InitHUD` of a client with `ScoreboardV1` (while the extension is inactive
  only `Reset` is sent);
- when `ScoreboardV1` appears after `InitHUD`;
- for every ready client, when a plugin turns the extension on in the middle of a map (a client
  that is still in its 0.5 s wait after `InitHUD` keeps it).

`InitHUD` already sends `ScoreInfo` and `TeamInfo` for every player and the MOTD. The engine
moves a client's reliable buffer (about 4 KB on HLDS) out only after the previous reliable message
is acknowledged, so the sync is paced by time, not by frames, to avoid `Reliable channel
overflowed`: it starts after the `InitHUD` burst and drains at about 5 KB/s. With 32 players a
GunGame sync is about 3.3 KB and takes about 0.7 s.

### 5.4 Per-player texts and visibility

Templates and element visibility have a default and optional overrides for single players:

- A write for player 0 sets the default and drops the overrides of the same template variant, or
  of the same visibility bit, for every player: the last write wins. It always drops them, even
  when the default doesn't change. Only clients whose effective value changed get a message.
- A write for player N sets an override for that client only. An override equal to the default
  is dropped.
- Each client gets its override if it has one, otherwise the default.
- Overrides are cleared when the player slot is cleared (connect, disconnect), when the element is
  removed, on reset and on map change. A kind change clears the text overrides of the element and
  keeps the visibility ones (9.2).
- The overrides are kept apart from the per-client sync state, because a full sync resets that
  state.

The hidden standard HUD parts have no overrides.

### 5.5 Events

| Event                       | Server                                                                                    |
|-----------------------------|-------------------------------------------------------------------------------------------|
| Map change                  | State and event queues are cleared; the plugin sets everything again                      |
| Plugin resets the extension | `Reset` to every ready client; event queues are cleared; the extension is off             |
| Plugin removes an element   | `Element` with kind `None`; `0xFF` when it removes all                                    |
| Player disconnects          | Slot cells, overrides and event queue are cleared; `0xFF`/`None` for the slot to everyone |
| Player changes team         | Nothing: placement comes from `TeamInfo` and `ScoreInfo`                                  |
| Client loses `ScoreboardV1` | Nothing more is sent to that client                                                       |

A client dropped before it spawned (while it loads the map or downloads) counts as a disconnect
too; the server notices it in the next frame.

### 5.6 Event queue

`HudEvent` (`bhl_hud_event`, 9.2) doesn't change the state. It builds one `Event` and puts it into
the queues of the clients that get it, at the moment of the call:

- player 0: every client that is ready, has `ScoreboardV1` and `HudV1`; player N: that client only,
  if so. A client that gets these caps or becomes ready later never gets the event.
- Each client has a queue of 8 events, kept apart from its sync state. When it is full, the oldest
  event is dropped.
- The event names its element by key. The key is looked up when the event is sent, after
  `HudState` (5.2), within the same budget. The first event of the queue waits, and the events
  after it with it, while that element doesn't exist, is not a `Feed` or was not sent to this
  client yet (for example during its full sync).
- An event older than its life is dropped unsent: `life`, or for 0 the `life_ms` param the plugin
  set on the element, else its default from `PARAM_INFO` (5000 ms).
- The queues are cleared on map change, on `bhl_sb_reset()` and when the slot is cleared (connect,
  disconnect). A full sync neither clears nor replays them.
- The server refuses an event longer than 192 bytes; with the natives' arguments it never is.

## 6. Client behaviour: scoreboard

### 6.1 Store

The store, `CScoreboardData`, holds the schema, cells, lists, elements, templates and the HUD
state. It is cleared on `VidInit`, on `Reset` and when the client clears `ScoreboardV1` in `_bhl`
(3.2). A player's cells are cleared when the slot becomes empty, even if the server didn't say so.

### 6.2 Activation

The custom scoreboard is shown when a `Config` arrived after the last `Reset` or `VidInit` and
`hud_scoreboard_custom` is 1. Otherwise the standard scoreboard is shown; incoming data is still
stored and HUD elements still use it.

### 6.3 Layout

- Column order: `rank`, Avatar, Name, then SteamID, Eff, Frags and Deaths at their default places
  unless the schema moves them, then custom and `Builtin` columns by `id`, then Ping unless moved.
  Hidden standard columns, `Hidden` custom columns and `Badge` columns are left out.
- The panel keeps the size, place, frame and header of the standard scoreboard (`ScorePanel.res`).
- Name takes all free width. If the other columns need more, Name shrinks down to 100 units
  (`column_name_min`), then down to 80 units; columns that still don't fit are dropped from the
  end.
- While a `Steps` column is visible, rows are taller so the strip fits under the text.
- The content of a custom cell (and the title of a custom column) leaves `cell_margin` (6 units)
  free at its right, so a strip or a long text doesn't touch the next column.
- Team section headers show team cells in the same columns as text only, without icons, dots or
  strips: `Icon` as its name, `Progress` as `cur/max`. A `Meter` column with `max ≤ 10` stays
  empty, like dots without a counter. Badges of team cells follow their host cell as in player
  rows (6.7). Team frags and deaths follow the `Hide*` flags.

### 6.4 Sorting and places

- Players inside a section and the teams are sorted by the sort keys in priority order. Ties fall
  back to the standard order (frags descending, deaths ascending), then to the player index or the
  team number, ascending. Teams are sorted by team cells; a team without cells keeps the standard
  order.
- Team frags and deaths come from `TeamScore`; until the server sends one, they are the sums over
  the team's players, as in the standard scoreboard.
- Participants are connected players that are not spectators; bots count.
- The number of a player or a team is its first-key `Int`, `Progress.cur` or `Clock` seconds.
  Without sort keys it is its frags, and larger is better.
- The place in the `rank` column depends only on the first sort key: 1 + the number of entities
  whose first-key cell sorts strictly before, compared as the column sorts, so equal values share a
  place (`1, 1, 3`). An entity with an empty first-key cell has no place. Further keys only order
  the rows. Without sort keys equal frags share a place.
- Team mode: with sort keys, at least one team with participants has a non-empty first-key team
  cell; without sort keys, at least one participant is in a team. In team mode the ranked entities
  are the teams with participants, the first row is the first of them in team order, and the
  ranked entity of any player is his team. Otherwise the ranked entities are the participants, the
  first row is the first participant in player order, and the ranked entity of a player is the
  player.
- With team sections the place is shown in the team header and counted among teams; player rows
  leave `rank` empty. Without teams the place is shown in player rows.
- The store computes the order, so HUD elements with `Leader` (8.2) and `^` refs (8.10) work with
  the standard scoreboard too.

### 6.5 `hud_scores`

With an active schema, `hud_scores` uses the first `Primary` column (lowest `id`) that isn't
`Hidden`, `Builtin` or `Badge`. It shows that column as text instead of frags and sorts the same
way as the scoreboard. Without such a column it doesn't change.

### 6.6 Drawing

- `Icon`: the sprite from `hud.txt` or a sprite file, scaled to the row height with its aspect
  ratio kept and tinted with `cell_icon_color` (orange like the dots by default). The text follows
  to the right. The icon is drawn additively, like the HUD: with OpenGL through TriangleAPI, which
  sets its own blending (VGUI2 stops blending once a row has drawn text). The software renderer
  gets a VGUI2 texture that the client reads from the sprite file and scales to the drawn size
  itself; its pixels brighter than 48 are opaque and the rest transparent, because it is drawn
  without blending. HUD element icons are drawn the same way. With OpenGL `CHudRenderer` leaves
  blending off after a sprite, so `HudServerPanel` sets `kRenderTransTexture` again after each of
  its sprites: the shapes it draws later in the same pass (a `Box`, a `Board` plate) stay blended.
- Sprite files (4.3) are loaded the first time a value names them. The client reads and checks the
  file itself (an HL sprite whose first frame is a single frame of at most 1024 × 1024) and only
  then calls `SPR_Load`, so a missing or broken file never reaches the engine, which would stop
  with an error. The whole first frame is drawn. Paths are matched without case; up to 32
  different paths per game get an index, more are not drawn. A failure is remembered until the
  next `VidInit`, after which every file is loaded again on its next use.
- `Meter` keeps the dot look of the scoreboard, which is not the HUD `Meter` (8.7). Dots are discs
  of `cell_dot_size` with `cell_dot_gap` between them, smaller when the column or the row is too
  small for 10 of them:
  - `max ≤ 10`: `cur` dots in `cell_filled_color` and `max − cur` dim ones in `cell_empty_color`,
    without a counter;
  - `max > 10`: a rounded bar of the same length as 10 dots in `cell_empty_color`, filled to
    `cur / max` (at least one dot) in `cell_filled_color`, with the `cur/max` counter to the right.
- `Steps`: `cur/max` as text; below it a strip of `max` segments. Segments before `cur` are
  filled, segment `cur` is highlighted, the rest are dimmed. With `MarkLast` a dimmed last segment
  uses `cell_last_color`. When the column has less than 2 pixels per segment, the strip is one bar
  filled to `cur / max`.
- `Progress` in a `Text` column: `cur/max`.
- `Clock` in any column: written with `time` (8.10). A running clock is refreshed at least once a
  second while the scoreboard is open.
- Plugin strings (the title, column titles, cells, badges and banner rows) are drawn as they are:
  `&` is an ordinary character, not a VGUI2 hotkey mark.

### 6.7 Badges

- A non-empty cell of a `Badge` column is drawn as a badge after the content of the cell of
  column `host`, with the look of the tag badge (`badge_font`, `badge_bgcolor`, `badge_fgcolor`).
  This holds in player rows with player cells and in team headers with team cells, so in GunGame
  teamplay the `final` badge follows the team's weapon.
- The badge text is the cell drawn as text: a `#token` is localized.
- A badge is drawn even when the host cell is empty. In a narrow column the text of the host cell
  is cut first; badges that still don't fit are left out.
- No badge is drawn when the `Badge` column is `Hidden`, or when `host` is missing, hidden,
  `Builtin` or a `Badge` column, or is cut off by the layout. Several badges after one cell are
  drawn in `id` order.

### 6.8 Banner

The custom scoreboard has a banner in its header, left of the player count, on the rows of the
server name and the map name. Its two rows are `Text` elements placed `BannerTop` and
`BannerBottom`. The plugin decides what a row says and when, through templates, `select` and
visibility; the client knows no game mode.

- A row shows the lowest-id `Text` element with that place that is visible in `HudState` and whose
  text is not empty.
- `Subject` and `Leader` apply as on the screen (8.2). A row with `Leader` is not drawn when there
  are no participants. `x`, `y`, `align`, `gap`, `font` and `NoScoreboard` are ignored.
  `hud_server_elements` and the other rules from 8.3 don't apply: the banner belongs to the
  scoreboard.
- `Icon`, `Meter` and `Strip` elements with a banner place are not drawn.
- The banner is drawn when at least one of its rows is drawn.
- Look: a box (`banner_bgcolor` 255 64 32 48) with an accent bar on the left
  (`banner_accent_color` 255 160 0 255, `banner_accent_wide` 2, `banner_padding` 1). The rows use
  the fonts of the server name and the map name labels. With `HudColor` the top row uses
  `banner_accent_color` and the bottom row the header text color; otherwise rows use the element
  color.
- Layout:
  - the right edge is at the left edge of the player count text, minus the spacing and the padding;
  - rows are cut so that the map name and a third of the server name stay visible;
  - the server name is narrowed so that the name and the tag end before the banner;
  - the banner is laid out again whenever the evaluated text of a row changes, and on every change
    of the header (server name, map name, player count, tag).

### 6.9 Debug commands

- `hud_scoreboard_test [off]` fills the store with fake data for the current players without a
  server. It builds real `BhlScore` messages and feeds them to the parser: columns `rank`, `stage`
  (`Steps`, `MarkLast`), `item` (`Icon`), `progress` (`Meter`), hidden `score` as the first sort
  key, a badge after `item`, a list `items`, and elements that use the client tokens (place line,
  rival line, meter, strip, banner).
- `hud_scoreboard_dump` prints the store: schema, cells, lists, elements with their params and slot
  colors, templates with their parse errors, the HUD state, the subject and the built-in values.
- `hud_server_eval "<template>"` prints the template evaluated for the current subject and lists
  the missing refs.

## 7. Client behaviour: standard scoreboard

Without an active schema the scoreboard is the same as now. The place column appears only when a
schema adds the built-in `rank` column. With a schema the scoreboard keeps the standard look: size,
place, header, fonts, column titles and row highlights.

## 8. HUD elements

### 8.1 Model

The client draws elements in one full-screen VGUI2 panel, `HudServerPanel` (`CHudServerPanel`).
An element has:

- a kind: `Text`, `Icon`, `Meter`, `Strip`, `Board`, `Feed` or `Box`;
- a place and a position (8.4), a color, color slots and a font (8.5);
- flags (4.3);
- refs to the data it shows: `select`, `value` and `list`;
- kind parameters and the params of every kind (8.13), with defaults in `HudServerPanel` of
  `HudLayoutBase.res` that users can override in `HudLayout.res` (a param sent by the server wins);
- up to 8 template variants;
- a visibility bit set by the plugin.

Elements are drawn in two passes. The first pass draws every `Box` (8.16), then, in id order, the
shapes and sprites of all other elements (`Meter` dots, bars and rings, `Strip` cells, item icons
and track discs, `Icon` sprites, `Board` plates, fill bars and icons, `Feed` icons). The second pass
draws, in id order, all texts (`Text` elements, `Meter` counters, `Icon` texts, `Strip` item texts,
scroll markers and track counts, `Board` rows, `Feed` lines). A text is therefore always drawn above
any shape or sprite, and a `Box` below all of them, whatever the ids.

The client has no conditions and no game logic: the plugin shows and hides elements, and a choice
that depends on the subject comes from a value through `select` or from the missing-value rule
(8.10). Texts are evaluated every frame, so a changed cell needs no HUD traffic.

### 8.2 Subject

| Viewer                                                                           | Subject                          |
|----------------------------------------------------------------------------------|----------------------------------|
| Not observing (`g_iUser1 = OBS_NONE`)                                            | The local player                 |
| Spectator in `OBS_CHASE_LOCKED`, `OBS_CHASE_FREE`, `OBS_IN_EYE`, `OBS_MAP_CHASE` | The observed player (`g_iUser2`) |
| Spectator in `OBS_ROAMING` or `OBS_MAP_FREE`                                     | None                             |

- The subject must be connected and not a spectator. A dead player is still a subject.
- With `Leader` the subject is the first row (6.4): a team in team mode, otherwise a player.
  Without participants there is no subject.
- The ranked entity (8.11) of a player subject is his team in team mode (6.4), otherwise the
  player. A team subject is its own ranked entity. In team mode a player without a team is not
  ranked: `@place`, `@state`, `@behind`, `@score` and the rival values are missing.
- Without a subject, refs read global cells only and built-in values that need a subject are
  missing.

### 8.3 Visibility

An element on the screen (places `Screen`, `NumbersRow` and the parent places) is drawn when all of
these hold:

1. `hud_server_elements` is 1.
2. `hud_draw` is above 0, there is no intermission and no `HIDEHUD_ALL`.
3. Its bit is set in `HudState.visible`.
4. With `Subject` there is a subject; with `Leader` there are participants.
5. With `NoScoreboard` the scoreboard is closed.
6. It is placed (8.4).
7. It has content: a `Text` evaluates to a non-empty text; an `Icon` has an `Icon` value and either
   a sprite the client has or a non-empty text (8.9); a `Meter` value is `Progress` with `max > 0`;
   a `Strip` has `count > 0` and a numeric position; a `Board` has at least one row (8.14); a
   `Feed` has a live line (8.15); the group of a `Box` draws something (8.16).
8. With `hold_ms`, the hold since it appeared or changed has not run out (8.13).

An element with `fade_ms` that stops being drawn because of rules 3–8 fades out, drawn as it was
in its last frame (8.13). Rules 1 and 2, a removal and a kind change hide it at once.

Banner rows follow 6.8.

Visibility belongs to the viewer, not to the subject: a spectator sees the elements the plugin
shows to him, with the data of the observed player.

### 8.4 Placement

```
P(v) = (int)(v · H / 480)      proportional units, vgui2::scheme()->GetProportionalScaledValue
fx   = x / 8192                fy = y / 8192
W, H                           screen size; w, h: size of the element's box
```

Horizontal position, for places `Screen`, `Parent` and `NumbersRow`:

| `x`         | Reference point       | Auto anchor |
|-------------|-----------------------|-------------|
| `−8192`     | `W / 2`               | center      |
| other `< 0` | `(int)((1 + fx) · W)` | right edge  |
| `≥ 0`       | `(int)(fx · W)`       | left edge   |

An explicit anchor puts the left edge, the center or the right edge of the box on the reference
point. These rules are the rules of `set_hudmessage`.

Vertical position:

- `Screen`: the same rules with `y`, `fy` and `H`; the auto anchors are middle, bottom edge and top
  edge.
- `Parent`: the vertical anchor picks the side. Auto, top and middle put the child below the
  parent: `top = parent bottom + P(gap)`. Bottom puts it above: `bottom = parent top − P(gap)`.
  `y` is ignored.
- `ParentLeft`: the right edge of the box is at `parent left − P(gap)`. `ParentRight`: the left
  edge is at `parent right + P(gap)`. The box is centered vertically on the parent; `x`, `y` and
  the anchors are ignored.
- `NumbersRow`:
  - `rowH = gHUD.m_iFontHeight` (it follows `hud_scale`), `rowTop = H − rowH − rowH / 2`;
  - the side zones are `[0, 15 · digitW + P(gap))` and `(W − 15 · digitW − P(gap), W]`, where
    `digitW` is the width of the `number_0` sprite, and `free` is the width between them;
  - with `hud_custom 1` the left zone ends at the right edge of the health and battery panels plus
    `P(gap)`, the right zone starts at the left edge of the ammo panel minus `P(gap)`, and `rowTop`
    and `rowH` are the top and the height of these panels together. These zones are not
    symmetric: a centered box crosses the nearer zone before it fills `free`, so a long `Strip`
    is lifted above the row;
  - normally the box is centered on the row: `top = rowTop + (rowH − h) / 2`;
  - a `Strip` first shrinks to `free` (8.8); a box that then intersects a side zone where it is
    drawn (after it is moved inside the screen) is lifted: `bottom = rowTop − P(gap)`.

Every box is then moved inside the screen, like `set_hudmessage` does.

Order and fallbacks:

- Elements are laid out parents first, so a child uses the box of its parent after the parent was
  moved inside the screen.
- A parent's box counts even when the parent is not drawn: an empty `Text` box has the height of
  its font. Children don't move when a parent's text is empty or hidden.
- An element is unplaced when its parent is undefined, has a banner place, is a `Box`, is the
  element itself, is part of a cycle or is unplaced. A `Box` without a parent place is unplaced
  too. An unplaced element is not drawn and has no box.
- A `Box` takes its box from its group (8.16): `x`, `y`, the anchors and the rules of its place
  don't apply to it, and `gap` is its padding.

Sizes and gaps are proportional units of a 640×480 screen, `P()`, like `.res` files. They don't
follow `hud_scale`. Positions are screen fractions, so `y = 0.025` (205) gives the same pixel as
`P(12)` at the common screen heights.

### 8.5 Color and fonts

- Color: with `HudColor` the rgb comes from `hud_color` (`HudPart::Common`), otherwise from the
  message.
- Color slots (4.3) are set by `bhl_hud_slot_color` (9.2). A slot color is used as it is, r, g, b
  and a, also by an element with `HudColor`, and replaces the whole color while it applies:

  | Slot | Name     | Applies                                                                       |
  |------|----------|-------------------------------------------------------------------------------|
  | 0–7  | Variants | While that variant is drawn (see below)                                       |
  | 8    | `Warn`   | While `warn_at` holds (8.13); without the slot `255 64 32 255`                |
  | 9    | `Accent` | The accent bar of a `Box` (8.16); without the slot the `hud_color` rgb at 255 |
  | 10+  | —        | Skipped by the client                                                         |

  The drawn variant is the one `select` picks for a `Text`, always 0 for an `Icon` and a `Meter`,
  and the variant of each row of a `Board` and of each line of a `Feed`, which then have their own
  colors. A `Strip` and a `Box` use no variant slots.
- Order: the element color (or `hud_color`), the variant slot, the warn color, then the flash
  toward white (8.13). While warning, `Board` rows and `Feed` lines use the warn color too, not
  their variant slots.
- Alpha: `part · a / 255 · GetHudTransparency() · fade · pulse`, where `part` is 255 or the alpha
  of a `Meter`, `Strip` or `Board` part or of a `Feed` line, `a` is the alpha of the color in use,
  and `fade` and `pulse` come from `fade_ms` and `warn_pulse_ms` (8.13), 1 without them.
- Fonts are a fixed set of entries in `ClientSchemeBase.res`. Users can change them in
  `ClientScheme.res`.

  | Id | Font             | Look        | Tall at yres 480 / 600 / 768 / 1024 / 1200+ |
  |----|------------------|-------------|---------------------------------------------|
  | 0  | `HudServerText`  | Verdana 600 | 12 / 13 / 14 / 20 / 24                      |
  | 1  | `HudServerLarge` | Verdana 800 | 16 / 18 / 20 / 28 / 34                      |
  | 2  | `HudServerSmall` | Verdana 600 | 10 / 11 / 12 / 16 / 18                      |
  | 3  | `HudServerHuge`  | Verdana 800 | 24 / 28 / 32 / 44 / 52                      |

- Text is drawn through the VGUI2 surface, so Cyrillic and scaling work.

### 8.6 `Text`

- One line: the evaluated template of the chosen variant (8.10). Line breaks are drawn as spaces.
- The box is the text width × the font height, with width 0 for an empty text.
- No kind params; the params of every kind apply (8.13). The slot of the chosen variant colors the
  line (8.5).

### 8.7 `Meter`

`value` must be a `Progress` with `max > 0`; `cur` is clamped to `[0, max]`.

| Id | Param         | Default | Range | Meaning                                                               |
|----|---------------|---------|-------|-----------------------------------------------------------------------|
| 1  | `dots`        | 10      | 0–32  | `max ≤ dots`: dots; otherwise a bar. 0: always a bar                  |
| 2  | `dot_size`    | 7       | 2–64  | Diameter of a dot, height of the bar                                  |
| 3  | `dot_gap`     | 3       | 0–64  | Distance between dots                                                 |
| 4  | `bar_wide`    | 0       | 0–640 | Length of the bar; 0 is the length of `dots` dots, or 10 for 0 dots   |
| 5  | `counter`     | 1       | 0–2   | Counter: 0 never, 1 with the bar, 2 always                            |
| 6  | `counter_gap` | 6       | 0–64  | Distance between the box and the counter                              |
| 7  | `alpha_empty` | 80      | 0–255 | Empty part of the bar                                                 |
| 40 | `form`        | 0       | 0–1   | 0: dots or a bar in a line; 1: a ring of dots with the counter inside |

- After scaling a dot is `dot = max(2, P(dot_size))`.
- Dots: `max` dots of `dot` with `P(dot_gap)` between them. Dots before `cur` are filled
  anti-aliased discs, the rest are rings with a width of 1/6 of the diameter, all at alpha 255.
- Bar: a rounded bar (half-disc caps) at `alpha_empty`. When `cur > 0`, a fill of width
  `max(w · cur / max, dot)` at alpha 255.
- The box is the row of dots or the bar. Without a `Progress` value with `max > 0` the box is
  0 × `dot` and nothing is drawn. The counter stands outside the box, so the anchor keeps the box
  in place when the form changes.
- Ring (`form` 1), with `d = dot` and `g = P(dot_gap)`:
  - `n = max` dots when `max ≤ dots`, otherwise `dots`, or 10 for `dots` 0;
  - `filled = cur` when `n = max`, otherwise `cur · n / max` rounded half up;
  - the box is `D × D` with `D = max(3 · d, ceil(n · (d + g) / π) + d)`;
  - dot `i` has its center at the angle `−90° + 360° · i / n` (dot 0 on top, then clockwise) on a
    circle of radius `(D − d) / 2` around the center of the box, rounded to whole pixels;
  - the first `filled` dots are discs, the rest rings, as in a line of dots, all at alpha 255;
    `bar_wide` and `alpha_empty` are not used.
- Counter:
  - the text is variant 0, or `{@value:cur} / {@value:max}` when the element has no variant 0;
    variants 1–7 are never used;
  - `counter` 1 shows it with the bar, and in the ring form whenever `n ≠ max`;
  - it uses the element font and is centered vertically on the box;
  - its left edge is at `box right + P(counter_gap)`; when the resolved horizontal anchor of the
    box is the right edge (an explicit right anchor, an auto anchor with a right-edge reference
    point (8.4) or the place `ParentLeft`), its right edge is at `box left − P(counter_gap)`;
  - in the ring form it is centered in the ring instead, and `counter_gap` is not used.
- Slot 0 colors the dots, the bar, the ring and the counter (8.5).

### 8.8 `Strip`

| Id | Param               | Default | Range         | Meaning                                            |
|----|---------------------|---------|---------------|----------------------------------------------------|
| 10 | `cell_wide`         | 30      | 1–255         | Width of a cell                                    |
| 11 | `cell_min_wide`     | 14      | 1–`cell_wide` | Cells shrink down to this width                    |
| 12 | `cell_tall`         | 15      | 1–255         | Height of a cell                                   |
| 13 | `cell_gap`          | 3       | 0–64          | Distance between cells                             |
| 14 | `icon_padding`      | 2       | 0–64          | Padding of the icon inside a cell                  |
| 15 | `max_wide`          | 300     | 0–4096        | Width limit; 0 is the screen width                 |
| 16 | `alpha_passed`      | 112     | 0–255         | Cells before the current one                       |
| 17 | `alpha_current`     | 48      | 0–255         | Current cell                                       |
| 18 | `alpha_future`      | 24      | 0–255         | Cells after the current one                        |
| 19 | `alpha_icon_future` | 80      | 0–255         | Icons and texts of the cells after the current one |
| 20 | `frame`             | 1       | 0–1           | 1: the current cell has a 1 px frame at 255        |
| 41 | `track`             | 0       | 0–1           | 1: markers of the other players above their cells  |

Inputs:

- `count` is the number of items of `list`, or the `max` of the `value` `Progress` without a list
  (also when no list has that key); at most 64.
- The position is the `value`: `Progress.cur` or another number (8.10), 1-based.
  `cur = clamp(position − 1, 0, count − 1)`.
- Items:
  - an `Icon` item draws its sprite from `hud.txt` or a sprite file (4.3, 6.6), cached until the
    store changes; a sprite the client doesn't have leaves the cell empty;
  - any other item is drawn as text (8.10) centered in its cell, in the element font;
  - an empty item, or a cell without a list, is an empty cell.

Layout:

```
gap  = P(cell_gap)
cmin = max(1, P(cell_min_wide)); cmax = max(cmin, P(cell_wide))
mw   = P(max_wide), or W when max_wide is 0
free = place NumbersRow ? the width between the side zones (8.4) : infinite
minWide = count · cmin + (count − 1) · gap
lifted  = place NumbersRow && minWide > free && free < mw
maxWide = lifted ? mw : min(mw, free)
cell    = clamp((maxWide − (count − 1) · gap) / count, cmin, cmax)
shown = count; first = 0
if minWide > maxWide:
    marker = width of "‹" in the element font + gap
    shown  = clamp((maxWide − 2 · marker + gap) / max(cmin + gap, 1), 1, count)
    first  = clamp(cur − shown / 2, 0, count − shown)
box = shown · cell + (shown − 1) · gap  ×  P(cell_tall)
```

With `count = 0` these formulas are not used: the box is 0 × `P(cell_tall)` and nothing is drawn.

Drawing:

- Cells are filled rectangles: `i < cur` at `alpha_passed`, `i = cur` at `alpha_current`, the rest
  at `alpha_future`. With `frame` 1 the current cell has a 1 px frame at 255.
- An icon fits inside `cell − 2 · P(icon_padding)` with its aspect ratio kept and is centered. Icons
  and texts are drawn at 255, or at `alpha_icon_future` after the current cell.
- When cells are hidden at an end, `‹` stands left of the box or `›` right of it, at `gap`, in the
  element font, centered vertically on the cells.

Track (`track` 1):

- For every participant (6.4) other than the subject player, the client reads the `value` ref of
  the strip for that player, as the position is read for the subject, and counts the players on
  each cell: `clamp(position, 1, count) − 1`. A player without a number is not counted. With a team
  subject (`Leader` in team mode) every participant counts.
- Above every shown cell with `n > 0` players stands a marker, centered on the cell, its bottom
  `P(2)` above the top of the cell: `n` discs of `max(2, P(4))` with `P(2)` between them for `n` 1
  and 2, the text `×n` in the element font for 3 and more. Markers use the element color at 255.
- Like the counter of a `Meter`, the markers stand outside the box: the box and the layout don't
  change.
- The counts are computed every frame.

### 8.9 `Icon`

| Id | Param       | Default | Range | Meaning                                                 |
|----|-------------|---------|-------|---------------------------------------------------------|
| 30 | `icon_tall` | 0       | 0–255 | Height of the icon; 0 is the height of the element font |
| 31 | `icon_gap`  | 2       | 0–64  | Distance between the icon and the text                  |

- `value` must be an `Icon` cell. Its sprite from `hud.txt` or sprite file (4.3) is scaled to
  `P(icon_tall)` or the font height with its aspect ratio kept, tinted with the element color, or
  slot 0 (8.5), and drawn like the icons of the scoreboard (6.6).
- Variant 0, if the element has it, is drawn after the icon at `P(icon_gap)`, in the element font,
  centered vertically on the icon: `{@value}` writes the name of the icon (8.10). Without variant 0
  only the icon is drawn.
- If the client doesn't have the sprite, only the text is drawn.
- The box holds the icon, the gap and the text in a row and is as tall as the higher of the icon
  and the font. The gap counts only when both the icon and the text are drawn. Without an `Icon`
  value the element has no content, not even its text, and the box is 0 × the font height.

### 8.10 Templates

A template is a text with placeholders. The client evaluates it every frame for the element's
subject. The rows of a `Board` are evaluated for the subject of each row (8.14), and the line of
an `Event` once, on arrival (4.3).

```
template    := ( literal | "{{" | "}}" | placeholder )*
literal     := any character except "{" and "}", or a lone "}"
placeholder := "{" ref [ "?" ] ( ":" format )* ( "|" form )* "}"
ref         := [ "^" ] ( key | "@" name )
key         := [a-z0-9_]{1,15}
name        := [a-z]{1,14}
format      := [a-z0-9]+
form        := any characters except "{", "}" and "|"; "#" is the number, "##" a literal "#"
```

Parsing:

- A `{` that doesn't start a placeholder of this form is literal: `{}`, `{ @place }`, `{Имя}`.
  After a `{` without a closing `}` the rest of the template is literal.
- A template that is exactly a client token (4.2) is looked up once, when it arrives, and its
  localized text is evaluated as a template. The localized text is never looked up as a token
  again. A missing token is shown as its name without `#`.
- Known formats are `cur`, `max`, `left`, `time`, `time2`, `name`, `team` and a number. An unknown
  format or built-in value makes the value missing. New built-in values and formats need a new caps
  bit, so a plugin can tell which clients understand them. The built-in values that draft 3 adds
  (8.11) come before the release of `HudV1` and need none.
- A placeholder takes at most one of `cur`, `max` and `left`, at most one of `time`, `time2`,
  `name`, `team` and word forms, and at most one cut, a number from 1 to 255. A repeated or
  conflicting format, a cut out of range or more than 16 placeholders is a parse error: the
  variant draws nothing and `hud_scoreboard_dump` shows the error.
- The client parses a template once when it arrives.

Refs:

| Ref                   | Value                                                                    |
|-----------------------|--------------------------------------------------------------------------|
| `key`                 | Cell of the column with this key                                         |
| `@value`              | The element's `value` ref                                                |
| `@place`, `@rival`, … | Other built-in values (8.11)                                             |
| `@item`, `@a`, …      | Values of a `Board` row or of an event (8.11); missing elsewhere         |
| `^ref`                | The ref read for the first row (6.4) instead of the subject: `{^weapon}` |

A cell is looked up in the subject's player cells, then in the cells of the subject's team (his
team number from `ScoreInfo`, in team mode or not), then in the global cells. A team subject
starts with its team cells. Without a subject only global cells are read. A `^` ref reads the
cells and built-in values of the first row, `{^@name}` is the leader's name, and it is missing
when there are no participants.

Values are written as:

| Value       | Text                                                           |
|-------------|----------------------------------------------------------------|
| `Int`       | The number                                                     |
| `Text`      | The text; a client token is localized                          |
| `Icon`      | The weapon name for standard HL sprites (10.3), else the text  |
| `Progress`  | `cur/max`, or `cur` when `max = 0`                             |
| `Clock`     | As with `time`                                                 |
| Player name | The display name, never looked up as a token                   |
| Team name   | As the team header shows it: a `#` name VGUI2 has is localized |

Names are written by `@name`, `@rival` and the `name` and `team` formats, always without color
codes.

Formats:

| Format  | Result                                                                                        |
|---------|-----------------------------------------------------------------------------------------------|
| `cur`   | `Progress.cur`                                                                                |
| `max`   | `Progress.max`                                                                                |
| `left`  | `max(max − cur, 0)` of a `Progress`                                                           |
| `time`  | A number of seconds as `m:ss`, from one hour as `h:mm:ss`, below 0 as `0:00`                  |
| `time2` | Milliseconds as `m:ss.cc` the same way: an `Int`, `Progress.cur` or the current `Clock` value |
| `name`  | A number as the name of player N                                                              |
| `team`  | A number as the name of team N (numbered as in `ScoreInfo`)                                   |
| N       | Cut to N code points; `…` is added only when the text was cut                                 |

The formats apply in this order, whatever order they are written in: `cur`, `max` or `left`, then
word forms or `time`, `time2`, `name` or `team`, then the cut. A number is an `Int`, a
`Progress.cur` or the number of a `Clock` (4.3). `time2` rounds down to hundredths, or up for a
`Clock` that counts down.

Word forms `{ref|form1|form2|form3}` write one form chosen by the number; `#` in a form is replaced
by the number with its sign, while the rule uses its absolute value (10.2). The number of forms
selects the rule. With `@pass` = 3, `{@pass} {@pass|kill|kills}` and `{@pass|# kill|# kills}`
both give `3 kills`.

Missing values:

- An unknown key, an unknown built-in value or format, an empty cell, a missing built-in value, a
  format that doesn't fit the value, word forms on a value that is not a number, `name` for a slot
  without a player or `team` for a team without players make the value missing.
- A missing value makes the whole template empty, and a `Text` element draws nothing. So
  `PLACE {@place} / {@total}` disappears instead of showing `PLACE  / 8`.
- `{ref?}` writes a missing value as `""` instead.

Robustness:

- The result is cut to 255 UTF-16 units. Line breaks in it become spaces, on the screen and in the
  banner alike. A TAB of the template is kept: it separates the columns of a `Board` row (8.14). A
  TAB inside a value, such as a player name, becomes a space.
- Cell texts and localized cell tokens are never evaluated as templates, so player names and plugin
  values can't inject placeholders. A literal brace is written `{{` or `}}`. A plugin that formats
  a server string, such as a map or config name, into a template escapes it with
  `bhl_hud_escape()` (9.2).

Variants:

- `Text`: `select` `""` picks variant 0. Otherwise the number of the `select` ref picks variant
  0–7, for example `@state` or `level:left`. A missing value, a number out of range or a variant
  without a template draws nothing.
- `Icon`: variant 0 is the text after the icon (8.9).
- `Meter`: variant 0 is the counter (8.7); variants 1–7 are not used.
- `Board`: variant 0 is a row, variant 1 the own row when the element has it (8.14); variants 2–7
  are not used.
- `Feed`: each event names the variant of its line (4.3).
- `Strip` and `Box`: templates are not used.

### 8.11 Built-in values

The ranking follows the `rank` column (6.4):

- the ranked entities are the participants, or in team mode (6.4) the teams with participants;
- the number of an entity is defined in 6.4; entities without a number are not ranked;
- with a first key that is not a number (`Text`, `Icon`) nothing is ranked, although the `rank`
  column still shows places;
- `sign` is −1 for an ascending key, else 1; better means a larger `number · sign`;
- rivals are the other ranked entities.

| Name      | Value                                                                                             | Missing when                                   |
|-----------|---------------------------------------------------------------------------------------------------|------------------------------------------------|
| `@place`  | 1 + the number of entities strictly better; equal numbers share (`1, 1, 3`)                       | No subject, or its ranked entity isn't ranked  |
| `@total`  | Number of entities, with or without a number                                                      | Never                                          |
| `@state`  | 0 no rivals, 1 a rival ahead, 2 tied for the first place, 3 leading                               | Same as `@place`                               |
| `@rival`  | Name of the nearest rival ahead, the one with the smallest gap                                    | `@state` is not 1                              |
| `@gap`    | `(rival − own) · sign`, above 0                                                                   | `@state` is not 1                              |
| `@pass`   | `@gap + 1`: what the subject needs to pass the rival                                              | `@state` is not 1                              |
| `@lead`   | Smallest `(own − rival) · sign` over the rivals, 0 or more                                        | `@state` is not 2 or 3                         |
| `@behind` | `(best − own) · sign`, where `best` is the best number; 0 when the subject shares the first place | Same as `@place`                               |
| `@score`  | The number of the subject's ranked entity                                                         | No subject, or its ranked entity has no number |
| `@frags`  | Frags from `ScoreInfo`, or the team frags (6.4) for a team subject                                | No subject                                     |
| `@deaths` | Deaths, the same way                                                                              | No subject                                     |
| `@name`   | Name of the subject: the player, or the team for a team subject                                   | No subject                                     |
| `@value`  | The element's `value`                                                                             | No `value` or an empty cell                    |

- Among rivals with the same gap, `@rival` is the one that comes last in the scoreboard order, the
  row right above the subject.
- `@value` is resolved one level only. A `value` ref that is itself `@value`, with or without `^`
  or a format, is missing everywhere: in templates, in `select` and as the value a `Meter`, an
  `Icon` or a `Strip` draws. The server accepts such a ref.
- Built-in values are computed once per frame for each subject.

These built-in values exist only while the client evaluates a `Board` row or an event. Everywhere
else, in `select` and `value` and with `^`, they are missing:

| Name                   | Value                                                                                     | Where                |
|------------------------|-------------------------------------------------------------------------------------------|----------------------|
| `@index`               | Number of the row in the board, 1-based                                                   | `Board` rows         |
| `@item`                | The list item of the row, written like a cell value                                       | `Board` in list mode |
| `@value`               | The item at the row's index of the list that the `value` key names, with the ref format   | `Board` in list mode |
| `@a`, `@b`, `@c`, `@d` | Args 0–3 of the event by position, written like cell values: `{@a:name}` is a player name | Event lines          |
| `@icon`                | The first `Icon` arg, written like an `Icon` cell: the weapon name or its text            | Event lines          |

- In a ranking `Board` `@value` is the element's `value` read for the row subject, as usual. In a
  list `Board` with a `value` key, `@value` reads that key as a list key; a missing list or item
  makes it missing. A built-in `value` ref (`@…`) keeps its usual meaning.
- Formats apply to these values as to any other: `{@a:name}`, `{@item:20}`, `{@value:left}`.

A deathmatch line needs no columns, because without sort keys the ranking uses frags. One `Text`
element with `select @state`:

| Variant | Template                                                            |
|---------|---------------------------------------------------------------------|
| 0       | `Waiting for opponents`                                             |
| 1       | `Leader {^@name:20}: {^@score} · {@behind\|# frag\|# frags} behind` |
| 2       | `Tied for the lead`                                                 |
| 3       | `Lead: {@lead\|# frag\|# frags}`                                    |

### 8.12 Standard HUD parts

| Bit  | Name           | Hides                                                                 |
|------|----------------|-----------------------------------------------------------------------|
| 0    | `Timer`        | `hud_timer` modes 1–3                                                 |
| 1    | `NextMap`      | The `hud_nextmap` line                                                |
| 2    | `CustomTimers` | The display of `customtimer` timers; the sound at the end still plays |
| 3    | `Speedometer`  | `hud_speedometer` in its bottom position, not below the crosshair     |
| 4    | `JumpSpeed`    | `hud_jumpspeed` in its bottom position, not below the crosshair       |
| 5–15 | reserved       | Sent as 0, ignored                                                    |

- The bits come in `HudState.hide` and are honoured while `hud_server_elements` is 1.
- `NextMap` doesn't cover the AG `Nextmap` message, which shows the next map for 10 s when
  `hud_nextmap` is neither 1 nor 2.
- A bottom speedometer keeps its place above a bottom `hud_jumpspeed` even while `JumpSpeed` hides
  it.
- The client never moves standard HUD parts for elements. A plugin avoids overlaps by placing its
  elements or by hiding parts. The status bar (names of aimed players) is never hidden; with
  `hud_centerid 1` it can cross a lifted `NumbersRow` element.

### 8.13 Params of every kind

| Id | Param           | Default | Range   | Meaning                                                            |
|----|-----------------|---------|---------|--------------------------------------------------------------------|
| 32 | `flash_ms`      | 0       | 0–5000  | The color flashes toward white when the content changes            |
| 33 | `fade_ms`       | 0       | 0–5000  | The element fades in and out                                       |
| 34 | `hold_ms`       | 0       | 0–30000 | Drawn this long after it appears or changes; 0: always             |
| 35 | `warn_at`       | 0       | 0–32767 | The warn color while the number of `value` is at most this; 0: off |
| 36 | `warn_pulse_ms` | 0       | 0–5000  | Period of the alpha pulse while warning; 0: no pulse               |

Every kind takes them, and their defaults leave an element as it was. The client keeps the state
of the effects per element id between frames; a removal or a kind change resets it. Times are the
client's.

The content of an element, for `flash_ms` and `hold_ms`, is what it draws:

| Kind          | Content                                      |
|---------------|----------------------------------------------|
| `Text`        | The chosen variant and the evaluated text    |
| `Icon`        | The sprite and the text                      |
| `Meter`       | `cur`, `max` and the counter text when drawn |
| `Strip`       | The current cell and `count`                 |
| `Board`       | The texts of all rows                        |
| `Feed`, `Box` | Nothing: their content never changes         |

A template with a running `Clock` (`time`, `time2`) changes its text every second or every frame,
so a plugin doesn't combine such a text with `flash_ms` or `hold_ms`.

- `flash_ms`: when the content changes while the element is drawn and its subject stays, its rgb
  goes toward white and back: `c + (255 − c) · k` with `k = 1 − t / flash_ms`, `t` since the
  change. The variant colors of `Board` rows flash too. There is no flash when the element
  appears, when its subject changes (a spectator switches players), for a change while it was not
  drawn, and for the content after a `Reset` (so after a full sync) or `VidInit`.
- `fade_ms`: the alpha factor of the element goes from 0 to 1 over `fade_ms` when it starts being
  drawn, and from 1 to 0 when it stops; a reversal continues from the current factor. While it
  fades out, the element is drawn as it was in its last drawn frame; a `Strip` keeps its geometry
  but draws its current list items, icons and track markers. Rules 1 and 2 of 8.3, the removal of
  the element and a kind change cut it at once.
- `hold_ms`: after the element appears (it starts to pass rules 3–7 of 8.3, or its subject changes)
  or its content changes, it is drawn for `hold_ms`, then it counts as hidden (rule 8) and fades out
  with `fade_ms`. The next change of its content, or a new appearance, shows it again.
- `warn_at`: while the element passes rules 3–7 and the number of its `value` for its subject (an
  `Int`, `Progress.cur` or the seconds of a `Clock`) is at most `warn_at`, the element warns: its
  color is slot 8, or `255 64 32 255` without it (8.5). A missing value or one that is not a number
  doesn't warn.
- `warn_pulse_ms`: while warning, the alpha is multiplied by `0.55 + 0.45 · cos(2π · t /
  warn_pulse_ms)`, `t` since the warning started.

### 8.14 `Board`

| Id | Param             | Default | Range | Meaning                                                                       |
|----|-------------------|---------|-------|-------------------------------------------------------------------------------|
| 44 | `rows`            | 3       | 1–16  | Number of rows                                                                |
| 45 | `board_mode`      | 0       | 0–2   | 0: the first rows; 1: also the own row after them; 2: rows around the own row |
| 46 | `row_gap`         | 2       | 0–64  | Distance between rows                                                         |
| 47 | `column_gap`      | 8       | 0–64  | Distance between columns                                                      |
| 48 | `fill_alpha`      | 0       | 0–255 | Bar behind a row by its value; 0: none                                        |
| 49 | `highlight_alpha` | 48      | 0–255 | Plate behind the own row; 0: none                                             |

Sources:

- Ranking, `list` `""`: the rows are the ranked entities (6.4) in the scoreboard order, the
  participants or in team mode the teams with participants. The subject of a row is its entity.
  The own row is the row of the ranked entity of the element's subject (8.2).
- List, `list` names a list: the rows are its items, and the subject of every row is the element's
  subject. The own row is row `n`, 1-based, where `n` is the number of the `select` ref for the
  element's subject. A missing list has no rows.
- Without a subject there is no own row.

Rows, with `N = rows`:

- `board_mode` 0: the first `N` rows.
- `board_mode` 1: the first `N` rows and, when the own row is not among them, a separator row `…`
  and the own row after them, so up to `N + 2` lines.
- `board_mode` 2: `N` rows around the own row, `first = clamp(own − N / 2, 0, count − N)`; the first
  `N` rows without an own row.

Row text:

- The template is variant 1 for the own row when the element has variant 1, otherwise variant 0.
  It is evaluated for the subject of the row with `@index` and, in list mode, `@item` and `@value`
  (8.11).
- The text is split at TAB characters into at most 4 columns; further TABs become spaces.
- Each column is as wide as its widest text over the drawn rows, and columns are `P(column_gap)`
  apart. A column whose non-empty texts over the drawn rows have only digits, spaces and
  `+ - . , / : %` is right-aligned, any other is left-aligned.
- A row uses the element color or the slot of its variant (8.5). The separator `…` stands in the
  first column, in the color of variant 0.

Icons:

- Ranking: the row's `value`, read for the row subject, when it is an `Icon`. List: the item when
  it is an `Icon`.
- An icon is as tall as the font, with its aspect ratio kept, and centered in an icon column right
  before the last text column. That column is as wide as the widest icon and is followed by
  `P(column_gap)`; without icons there is none. A sprite the client doesn't have leaves its place
  empty. Icons are tinted with the row color.

Fill and highlight:

- With `fill_alpha > 0` a row gets a bar from its left edge, `f` of the row width, at `fill_alpha`
  in the element color. The value of a row is its `value` read for the row subject, or in list mode
  with a `value` key its `@value`. `f` is `cur / max` of a `Progress`, or for an `Int` the value
  divided by the largest `Int` over the drawn rows; a value of 0 or less, or of another type, gives
  no bar.
- With `highlight_alpha > 0` the own row gets a plate of the row size at `highlight_alpha` in the
  element color, under the fill bar.

Box: rows are as tall as the font and `P(row_gap)` apart; the box is as wide as the columns, the
icon column and the gaps, and `n · tall + (n − 1) · P(row_gap)` high for `n` lines. A board without
rows has no content.

### 8.15 `Feed`

| Id | Param          | Default | Range     | Meaning                                                    |
|----|----------------|---------|-----------|------------------------------------------------------------|
| 52 | `lines`        | 4       | 1–8       | Lines shown at most with `queue` 0                         |
| 53 | `life_ms`      | 5000    | 100–30000 | Life of a line whose event gives 0                         |
| 54 | `line_fade_ms` | 400     | 0–5000    | A line fades in and out over this time                     |
| 55 | `queue`        | 0       | 0–1       | 0: the newest lines on top; 1: one line at a time in order |
| 56 | `line_gap`     | 2       | 0–64      | Distance between lines                                     |

- The lines come only from `Event` messages (4.3). A line arrives at the client time of its event
  and lives for the event's `life`, or `life_ms` for 0. The age of a line runs while the element is
  hidden.
- `queue` 0: the live lines (`age < life`), newest first, at most `lines`.
- `queue` 1, an announcer: the lines in arrival order, each from `start = max(arrival, end of the
  previous line)` to `end = start + life`; the line whose interval holds now is shown alone.
- Alpha of a line: `min(1, t / fade, (life − t) / fade)` with `fade = line_fade_ms` and `t` the
  time since the arrival (`queue` 0) or the start (`queue` 1): it fades in at the beginning and out
  at the end of its life.
- A line is its icon (the sprite of the event's first `Icon` arg), as tall as the font with its
  aspect ratio kept, then `P(4)` when both are drawn, then its text in the element font. The line
  uses the element color or the slot of its variant (8.5), the icon too.
- The box is as wide as the widest shown line and `n · tall + (n − 1) · P(line_gap)` high. Lines are
  stacked from the top of the box, the newest first, and each is aligned in the box by the resolved
  horizontal anchor: left, center or right. The box is placed by its anchors as usual: with a top
  anchor older lines move down when a line arrives, with a bottom anchor the box grows upward.
- A feed has content while it shows a line, so it draws nothing between events.

### 8.16 `Box`

| Id | Param         | Default | Range | Meaning                                           |
|----|---------------|---------|-------|---------------------------------------------------|
| 60 | `accent_wide` | 0       | 0–64  | Width of the accent bar on the left edge; 0: none |
| 61 | `radius`      | 0       | 0–32  | Radius of the corners                             |

- A `Box` names the root of a group as its parent, with a parent place (`Parent`, `ParentLeft` or
  `ParentRight`); without one it is unplaced. `x`, `y`, the anchors and the rules of the place are
  ignored. No element can be placed at a `Box`, and a `Box` is never a parent (8.4).
- The group: the drawn elements whose parent chain reaches the root, the root included and `Box`
  elements excluded; elements that fade out count.
- The box is the union of their boxes and of what they draw outside them (the counter of a `Meter`,
  the scroll and track markers of a `Strip`), padded by `P(gap)` on every side.
- The `Box` is drawn when it passes the rules of 8.3, its content being a non-empty union.
- Before any other shape (8.1) it draws a filled rectangle with corners of radius
  `min(P(radius), w / 2, h / 2)`, line by line like the rounded boxes of the scoreboard, in the
  element color; then, with `accent_wide > 0`, the left `P(accent_wide)` pixels of the same shape
  in the slot 9 color, or the `hud_color` rgb at alpha 255 without it.
- Its alpha follows its own effects (8.13) and is multiplied by the largest fade factor of the
  group, so the box fades with its elements.
- A new element follows `hud_color` at alpha 255 (9.2): a plugin gives a `Box` its own color,
  usually a dark translucent one.

## 9. Plugin API

### 9.1 Server interface

`IBugfixedServer` gets minor version 4. Version 1.3 appended two methods:

- `GetClientCaps(int idx)` returns the caps of a client;
- `GetScoreboard()` returns `bhl::IScoreboardExt *`, a separate interface whose methods take
  column, list and element keys, never ids. New methods are appended to it the same way.

Version 1.4 appends `HudSetSlotColor` and `HudEvent` to the end of `IScoreboardExt`. Draft 1 builds
report 1.2 with an older `IScoreboardExt`, and calling it with the new layout would crash the
server; draft 2 builds report 1.3 without the two new methods, and calling them would call missing
vtable slots. So the module requires 1.4 for every scoreboard and HUD native.

`IScoreboardExt` is not released, so `SetConfig` changes in place, and `SetClock`, `AddBadge` and
the HUD methods follow the list methods:

```cpp
virtual void SetConfig(unsigned int hideFlags, const char *title, const char *tag) = 0;

// ... columns, cells and lists as before ...

virtual void SetClock(int target, const char *key, int ms, unsigned int flags) = 0;

//! host may name a column that is added later
virtual bool AddBadge(const char *key, const char *host) = 0;

//! Adds an element, or changes kind, position, font, anchors and flags of an existing one.
//! x and y follow set_hudmessage. A kind change drops templates, refs, params and slot colors.
//! Bit 0 of flags is ignored: the color mode comes from HudSetColor.
virtual bool HudAdd(const char *key, int kind, float x, float y, int font, int align, unsigned int flags) = 0;

//! parent may name an element that is added later
virtual bool HudSetPlace(const char *key, int place, const char *parent, int gap) = 0;

//! r < 0: follow hud_color
virtual bool HudSetColor(const char *key, int r, int g, int b, int a) = 0;

//! false, and nothing changes, when the Element message would exceed 192 bytes
virtual bool HudBind(const char *key, const char *value, const char *list, const char *select) = 0;

//! A param of the element's kind or of every kind (8.13); false as HudBind
virtual bool HudSetParam(const char *key, int param, int value) = 0;

//! player 0: the default, drops the per-player texts of the variant; otherwise that player only.
//! false for a text longer than 188 bytes, which is never cut.
virtual bool HudSetText(int player, const char *key, int variant, const char *text) = 0;

//! player 0: the default, drops the per-player visibility; otherwise that player only
virtual bool HudShow(int player, const char *key, bool show) = 0;

//! "" removes every element
virtual void HudRemove(const char *key) = 0;

//! Sets or clears only the given parts
virtual void HudHideStd(unsigned int parts, bool hide) = 0;

// Added in 1.4

//! slot 0-9; r < 0 clears the slot. Components are clamped to 0-255. false as HudBind.
virtual bool HudSetSlotColor(const char *key, int slot, int r, int g, int b, int a) = 0;

//! A line for a Feed element (5.6). player 0: every client with HudV1 now. lifeMs is clamped to
//! 0-65535, 0 for the element's life_ms. argCount 0-4 Int args; the icon, when its sprite or text
//! is not empty, is appended as an Icon arg, both cut to 31 bytes.
virtual bool HudEvent(int player, const char *key, int variant, int lifeMs, const int *args, int argCount, const char *iconSprite, const char *iconText) = 0;
```

`bugfixedapi_amxx` checks the interface version per feature instead of refusing the whole API.
On a server older than 1.4 the existing natives keep working, `bhl_sb_available()` returns false,
`bhl_get_client_caps()` returns 0 and the other `bhl_sb_*` and `bhl_hud_*` natives do nothing; the
first of them called on a map logs one error.

### 9.2 Natives

```pawn
enum BhlClientCaps
{
	BHL_CAPS_NONE = 0,
	BHL_CAPS_SCOREBOARD_V1 = (1 << 0),
	BHL_CAPS_HUD_V1 = (1 << 1)
};

enum BhlSbDisplay
{
	BHL_SB_TEXT = 0,
	BHL_SB_ICON = 1,
	BHL_SB_METER = 2,
	BHL_SB_BUILTIN = 3,
	BHL_SB_STEPS = 4,
	BHL_SB_BADGE = 5	// added by bhl_sb_add_badge only
};

enum BhlSbHide
{
	BHL_SB_HIDE_NONE = 0,
	BHL_SB_HIDE_FRAGS = (1 << 0),
	BHL_SB_HIDE_DEATHS = (1 << 1),
	BHL_SB_HIDE_EFF = (1 << 2),
	BHL_SB_HIDE_STEAMID = (1 << 3)
};

enum BhlSbColumnFlags
{
	BHL_SB_COL_NONE = 0,
	BHL_SB_COL_HIDDEN = (1 << 0),
	BHL_SB_COL_CENTER = (1 << 1),
	BHL_SB_COL_RIGHT = (2 << 1),
	BHL_SB_COL_PRIMARY = (1 << 3),
	BHL_SB_COL_MARK_LAST = (1 << 4)
};

enum BhlSbTarget
{
	BHL_SB_INVALID = -1,
	BHL_SB_GLOBAL = 0
};

#define BHL_SB_SORT_ASC 0x80

enum BhlSbClock
{
	BHL_SB_CLOCK_STOP = 0,
	BHL_SB_CLOCK_RUN = (1 << 0),
	BHL_SB_CLOCK_DOWN = (1 << 1)
};

enum BhlHudKind
{
	BHL_HUD_TEXT = 1,
	BHL_HUD_METER = 2,
	BHL_HUD_STRIP = 3,
	BHL_HUD_ICON = 4,
	BHL_HUD_BOARD = 5,	// rows of the scoreboard order or of a list
	BHL_HUD_FEED = 6,	// lines of bhl_hud_event
	BHL_HUD_BOX = 7	// backdrop of a group, placed with a parent place at the root of the group
};

enum BhlHudFont
{
	BHL_HUD_FONT_TEXT = 0,
	BHL_HUD_FONT_LARGE = 1,
	BHL_HUD_FONT_SMALL = 2,
	BHL_HUD_FONT_HUGE = 3
};

enum BhlHudAlign
{
	BHL_HUD_AUTO = 0,
	BHL_HUD_LEFT = 1,
	BHL_HUD_HCENTER = 2,
	BHL_HUD_RIGHT = 3,
	BHL_HUD_TOP = (1 << 2),
	BHL_HUD_VCENTER = (2 << 2),
	BHL_HUD_BOTTOM = (3 << 2)
};

enum BhlHudFlags
{
	BHL_HUD_NONE = 0,
	BHL_HUD_SUBJECT = (1 << 1),
	BHL_HUD_NO_SCOREBOARD = (1 << 2),
	BHL_HUD_LEADER = (1 << 3),
	BHL_HUD_DEFAULT = (1 << 1) | (1 << 2)
};

enum BhlHudPlace
{
	BHL_HUD_SCREEN = 0,
	BHL_HUD_PARENT = 1,
	BHL_HUD_NUMBERS_ROW = 2,
	BHL_HUD_BANNER_TOP = 3,
	BHL_HUD_BANNER_BOTTOM = 4,
	BHL_HUD_PARENT_LEFT = 5,
	BHL_HUD_PARENT_RIGHT = 6
};

enum BhlHudParam
{
	BHL_HUD_P_DOTS = 1,
	BHL_HUD_P_DOT_SIZE = 2,
	BHL_HUD_P_DOT_GAP = 3,
	BHL_HUD_P_BAR_WIDE = 4,
	BHL_HUD_P_COUNTER = 5,
	BHL_HUD_P_COUNTER_GAP = 6,
	BHL_HUD_P_ALPHA_EMPTY = 7,
	BHL_HUD_P_CELL_WIDE = 10,
	BHL_HUD_P_CELL_MIN_WIDE = 11,
	BHL_HUD_P_CELL_TALL = 12,
	BHL_HUD_P_CELL_GAP = 13,
	BHL_HUD_P_ICON_PADDING = 14,
	BHL_HUD_P_MAX_WIDE = 15,
	BHL_HUD_P_ALPHA_PASSED = 16,
	BHL_HUD_P_ALPHA_CURRENT = 17,
	BHL_HUD_P_ALPHA_FUTURE = 18,
	BHL_HUD_P_ALPHA_ICON_FUTURE = 19,
	BHL_HUD_P_FRAME = 20,
	BHL_HUD_P_ICON_TALL = 30,
	BHL_HUD_P_ICON_GAP = 31,
	BHL_HUD_P_FLASH_MS = 32,	// every kind
	BHL_HUD_P_FADE_MS = 33,	// every kind
	BHL_HUD_P_HOLD_MS = 34,	// every kind
	BHL_HUD_P_WARN_AT = 35,	// every kind
	BHL_HUD_P_WARN_PULSE_MS = 36,	// every kind
	BHL_HUD_P_FORM = 40,	// 0 dots or bar, 1 ring
	BHL_HUD_P_TRACK = 41,	// 0 off, 1 marks of the other players
	BHL_HUD_P_ROWS = 44,
	BHL_HUD_P_BOARD_MODE = 45,	// 0 first rows, 1 first rows and the own row, 2 rows around the own row
	BHL_HUD_P_ROW_GAP = 46,
	BHL_HUD_P_COLUMN_GAP = 47,
	BHL_HUD_P_FILL_ALPHA = 48,
	BHL_HUD_P_HIGHLIGHT_ALPHA = 49,
	BHL_HUD_P_LINES = 52,
	BHL_HUD_P_LIFE_MS = 53,
	BHL_HUD_P_LINE_FADE_MS = 54,
	BHL_HUD_P_QUEUE = 55,	// 0 newest lines on top, 1 one line at a time
	BHL_HUD_P_LINE_GAP = 56,
	BHL_HUD_P_ACCENT_WIDE = 60,
	BHL_HUD_P_RADIUS = 61
};

enum BhlHudStd
{
	BHL_HUD_STD_NONE = 0,
	BHL_HUD_STD_TIMER = (1 << 0),
	BHL_HUD_STD_NEXTMAP = (1 << 1),
	BHL_HUD_STD_CUSTOM_TIMERS = (1 << 2),
	BHL_HUD_STD_SPEEDOMETER = (1 << 3),
	BHL_HUD_STD_JUMPSPEED = (1 << 4)
};

#define BHL_HUD_COLOR -1
#define BHL_HUD_MAX_TEXT 188

// Color slots of bhl_hud_slot_color after the text variants 0-7
#define BHL_HUD_SLOT_WARN 8
#define BHL_HUD_SLOT_ACCENT 9

stock BhlSbTarget:bhl_sb_player(idx)
{
	return BhlSbTarget:idx;
}

native BhlSbTarget:bhl_sb_team(const teamName[]);

native BhlClientCaps:bhl_get_client_caps(idx);
forward bhl_client_caps_changed(idx);

native bool:bhl_sb_available();
native bhl_sb_reset();
native bhl_sb_config(BhlSbHide:hide, const title[] = "", const tag[] = "");
native bool:bhl_sb_add_column(const key[], const title[], BhlSbDisplay:display = BHL_SB_TEXT,
                              width = 0, sort = 0, BhlSbColumnFlags:flags = BHL_SB_COL_NONE);
native bool:bhl_sb_add_badge(const key[], const host[]);

native bhl_sb_set_int(BhlSbTarget:target, const key[], value);
native bhl_sb_set_text(BhlSbTarget:target, const key[], const text[]);
native bhl_sb_set_icon(BhlSbTarget:target, const key[], const sprite[], const text[]);
native bhl_sb_set_progress(BhlSbTarget:target, const key[], cur, max);
native bhl_sb_set_clock(BhlSbTarget:target, const key[], ms, BhlSbClock:flags = BHL_SB_CLOCK_RUN);
native bhl_sb_clear(BhlSbTarget:target, const key[] = "");

native bool:bhl_sb_list_resize(const key[], count);
native bhl_sb_list_set_int(const key[], index, value);
native bhl_sb_list_set_text(const key[], index, const text[]);
native bhl_sb_list_set_icon(const key[], index, const sprite[], const text[]);

native bool:bhl_hud_add(const key[], BhlHudKind:kind, Float:x = -1.0, Float:y = -1.0,
                        BhlHudFont:font = BHL_HUD_FONT_TEXT, BhlHudAlign:align = BHL_HUD_AUTO,
                        BhlHudFlags:flags = BHL_HUD_DEFAULT);
native bool:bhl_hud_place(const key[], BhlHudPlace:place, const parent[] = "", gap = 0);
native bool:bhl_hud_color(const key[], r = BHL_HUD_COLOR, g = 0, b = 0, a = 255);
native bool:bhl_hud_bind(const key[], const value[] = "", const list[] = "", const select[] = "");
native bool:bhl_hud_param(const key[], BhlHudParam:param, value);
native bool:bhl_hud_text(player, const key[], variant, const fmt[], any:...);
native bool:bhl_hud_show(player, const key[], bool:show = true);
native bhl_hud_remove(const key[] = "");
native bhl_hud_hide_std(BhlHudStd:parts, bool:hide = true);
native bool:bhl_hud_slot_color(const key[], slot, r, g, b, a = 255);
native bool:bhl_hud_event(player, const key[], variant, life_ms = 0, const sprite[] = "",
                          const sprite_text[] = "", any:...);

stock bool:bhl_hud_text_lang(player, const key[], variant, const mlKey[])
{
	if (player)
		return bhl_hud_text(player, key, variant, "%L", player, mlKey);

	new players[32], num;
	get_players(players, num, "ch");
	bhl_hud_text(0, key, variant, "%L", LANG_SERVER, mlKey);

	for (new i; i < num; i++)
		bhl_hud_text(players[i], key, variant, "%L", players[i], mlKey);

	return true;
}

//
// Doubles { and } so that a server string formatted into a template stays literal.
// A too long string is cut on a UTF-8 character boundary. src and dst must be different arrays.
//
stock bhl_hud_escape(const src[], dst[], len)
{
	new i, j;

	for (; src[i] && j < len; i++)
	{
		if (src[i] == '{' || src[i] == '}')
		{
			if (j + 2 > len)
				break;

			dst[j++] = src[i];
		}

		dst[j++] = src[i];
	}

	// The next byte continues a character, so drop its copied part
	while (j > 0 && (src[i] & 0xC0) == 0x80)
	{
		i--;
		j--;
	}

	dst[j] = EOS;
	return j;
}
```

Scoreboard:

- Teams are addressed by name, the same name `get_user_team()` returns. `bhl_sb_team()` returns
  an opaque handle that stays valid until the map changes, or `BHL_SB_INVALID` for an unknown team.
- Values for players that are not connected are ignored.
- `sort` is a priority starting at 1; add `BHL_SB_SORT_ASC` for ascending order.
- A `BHL_SB_BUILTIN` column takes one of the keys from 4.3: `rank`, `frags`, `deaths`, `eff`,
  `steamid`, `ping`. The server accepts any other valid key, and the client ignores such a column.
- `bhl_sb_add_column()` returns false for an invalid key, for `BHL_SB_BADGE` or another display
  outside 0–4, and when 32 columns exist. `bhl_sb_add_badge()` adds a `Badge` column after the
  column `host`, which may be added later. Its cells are set with the usual `bhl_sb_set_*`
  natives.
- `bhl_sb_set_clock()` sets a `Clock` (4.3): `ms` is its value now. `BHL_SB_CLOCK_RUN` runs it,
  `BHL_SB_CLOCK_DOWN` makes it count down to 0.
- `bhl_sb_list_resize()` creates the list on first use. Setting an item out of range is ignored.
- `sprite` of `bhl_sb_set_icon()` and `bhl_sb_list_set_icon()` is a `hud.txt` name or the path of
  a sprite file, `sprites/<name>.spr` (4.3). The plugin precaches a file with `precache_generic`
  in `plugin_precache`.
- `bhl_sb_reset()` removes everything: schema, cells, lists, elements, texts, visibility, hidden
  standard parts and queued events.
- `bhl_get_client_caps()` returns the normalized caps (3.1): `BHL_CAPS_HUD_V1` only together with
  `BHL_CAPS_SCOREBOARD_V1`, and 0 for bots and HLTV. Like the other natives of the extension it
  needs server API 1.4 and returns 0 on an older server.

HUD:

- Element keys are up to 15 characters of `[a-z0-9_]`, in their own namespace. `bhl_hud_add()`
  returns false for an invalid key or kind, or when 32 elements exist. The other `bhl_hud_*`
  natives return false for an unknown key. A `bhl_hud_*` native that returns false logs one line
  for that call. `bhl_hud_remove()` ignores an unknown key and logs nothing.
- A new element is shown, is placed on the screen, follows `hud_color` with alpha 255 and has no
  refs, params, slot colors or texts. `bhl_hud_add()` on an existing key changes its kind,
  position, font, anchors and flags and keeps the rest; a kind change drops its texts, the
  per-player ones included, its refs, params and slot colors, and keeps its place, color and
  visibility. The color mode comes only from `bhl_hud_color()`.
- `x` and `y` work like in `set_hudmessage`; hl.so sends them in 1/8192, rounded (4.3).
- `bhl_hud_place()` takes the key of the parent, which may be added later. While no element has
  that key, the element is not drawn (8.4). The parent is ignored for places that are not parent
  places.
- `bhl_hud_color()` with `r = BHL_HUD_COLOR` follows `hud_color` and uses only `a`.
- `bhl_hud_bind()` sets the refs: `value` and `select` are a column key or a built-in value, with an
  optional `^` and one of `cur`, `max` and `left` (`@state`, `level:left`); `list` is a list key.
- `bhl_hud_param()` takes a param of the element's kind or of every kind (8.13) and clamps the
  value to the range of the param (8.7–8.9, 8.13–8.16); it returns false for a param of another
  kind.
- `bhl_hud_slot_color()` sets the color of slot 0–9 (8.5), used as it is, also with
  `BHL_HUD_COLOR`; `r = -1` clears the slot. Components are clamped to 0–255.
- `bhl_hud_bind()`, `bhl_hud_param()` and `bhl_hud_slot_color()` return false and change nothing
  when the `Element` message would exceed 192 bytes; with the current kinds it never does (4.3).
- `bhl_hud_event()` adds a line to a `BHL_HUD_FEED` element. The text variant is evaluated by each
  client once, with up to 4 numbers as `@a`–`@d` and the icon as `@icon` (8.11); the sprite is
  drawn before the line. `life_ms` 0 takes the `life_ms` param of the element. Lines are not kept:
  only players in game with `BHL_CAPS_HUD_V1` at the moment of the call get them, and a line that
  can't be sent before its life ends is dropped (5.6). The native returns false for an unknown
  element or one that is not a feed, a variant out of 0–7, a player out of range or not
  connected, and more than 4 numbers. As AMXX passes variadic args by reference, the numbers can
  be any expressions.
- `player` 0 means everyone and always drops the per-player values of that text variant or
  visibility bit (5.4); a player index sets them for that player only, and returns false for a
  player who is not connected. Per-player settings are dropped when the slot is cleared, so a
  plugin sets them again in `client_putinserver`.
- `bhl_hud_text()` formats like `format()`: `%L` works and `%%` writes `%`. With `%L` the
  dictionary text is a format too, so a literal `%` in it is written `%%`. A result longer than
  `BHL_HUD_MAX_TEXT` (188) bytes is not set: the native returns false and logs an error. Text typed
  by players must not be formatted into a template; use refs such as `@rival` or the `name` format
  instead. Server strings such as map or config names go through `bhl_hud_escape()`.
- `bhl_hud_text_lang()` sets a text from the plugin dictionary: for one player in his language,
  or for player 0 the default in the server language plus every player in his language.
- `bhl_hud_hide_std()` sets or clears only the given standard parts, for everyone, so plugins that
  hide different parts don't undo each other. `bhl_sb_reset()` clears all.
- Setting a cell, an item, a text, a visibility or the hidden parts to the value it already has
  sends nothing.
- `bhl_client_caps_changed` doesn't fire for the caps a player had when he joined, because they
  are known before he is in game. Plugins check `bhl_get_client_caps()` when they need it.

### 9.3 GunGame

The plugin keeps working without the module: `set_module_filter` and `set_native_filter` in
`plugin_natives` let it load, and every call is guarded by `bhlApi`. The `level * 100` frags hack
stays for old clients. A per-player counter keeps the kills of the warmup and then of the match
for the `kills` column.

Data:

| Key          | Kind                 | Target         | Type       | Meaning                                                      |
|--------------|----------------------|----------------|------------|--------------------------------------------------------------|
| `rank`       | builtin column       | —              | —          | Place, computed by the client                                |
| `level`      | `Steps`, `MarkLast`  | player or team | `Progress` | Current level, 1-based / number of levels                    |
| `weapon`     | `Icon`               | player or team | `Icon`     | Weapon of the current level                                  |
| `progress`   | `Meter`              | player or team | `Progress` | Kills on the current level / kills it needs                  |
| `final`      | badge after `weapon` | player or team | `Text`     | `#BHL_SB_Final` on the last level                            |
| `kills`      | `Text`               | player, team   | `Int`      | Kills in the warmup, then in the match                       |
| `score`      | `Text`, hidden       | player, team   | `Int`      | Progress P, first sort key in the match                      |
| `phase`      | `Text`, hidden       | the whole game | `Int`      | 0 game, 1 warmup, 2 start countdown, 3 finished              |
| `phase_left` | `Text`, hidden       | the whole game | `Int`      | Seconds left in the warmup or the countdown; empty otherwise |
| `levels`     | list                 | the whole game | `Icon`     | Weapon of every level, in order                              |

- P is the sum of kills needed on all passed levels plus kills on the current level. Kills per
  level differ (1–7 in the shipped configs) and teamplay multiplies them by
  `gg_teamplay_multigoal`. Ordering by P gives the same order as the plugin's `Sort_RankPos`
  (level, then kills). Every player on the last level is ahead of every player who isn't.
- `kills` counts kills of other players. Suicides and team kills don't count. The counter is reset
  together with the scores when the match starts.
- Levels are the same for everybody in the warmup. While `phase` is 1 or 2 the plugin sends the
  columns again: `level`, `weapon` and `progress` become `Hidden`, `kills` becomes visible,
  `Primary` and the first sort key, `score` the second one. When the match starts it sends the
  match columns: `kills` is the second sort key, `Hidden` without teamplay. In teamplay `kills`
  stays visible and `progress` is 60 units wide, so Ping still fits.
- Without teamplay `level`, `weapon`, `progress` and `final` are player cells. In teamplay the
  level is shared by the team, so they are team cells only: the team header shows them, with the
  `final` badge after the team's weapon (6.7), and player rows leave them empty and show `kills`.
  HUD elements read them through the team cells (8.10). `score` stays a player cell too, so
  players of a team are ordered by `kills`.

HUD elements:

| Key             | Kind               | Place                           | Refs                               | Variants                            | Shown         |
|-----------------|--------------------|---------------------------------|------------------------------------|-------------------------------------|---------------|
| `gg_place`      | `Text`, large font | top center, `y 0.025`           | `select phase`, `value phase_left` | place, warmup, countdown            | always        |
| `gg_box`        | `Box`              | at `gg_place`, padding 6        | —                                  | —                                   | always        |
| `gg_rival`      | `Text`             | under `gg_place`, gap 2         | `select @state`                    | waiting, overtake, tied, lead       | match         |
| `gg_kills`      | `Meter`            | under `gg_rival`, gap 6         | `value progress`                   | —                                   | match         |
| `gg_levels`     | `Strip`            | numbers row, gap 3              | `value level`, `list levels`       | —                                   | match         |
| `gg_top`        | `Board`            | left top, `x 0.015`, `y 0.025`  | `value weapon`                     | row                                 | match         |
| `gg_banner_top` | `Text`, `Leader`   | banner top                      | `select level:left`                | title                               | until the win |
| `gg_banner_bot` | `Text`, `Leader`   | banner bottom                   | `select level:left`                | leader line                         | until the win |
| `gg_feed`       | `Feed`, small font | right top, `x −0.015`, `y 0.22` | —                                  | last level, lost level, lead        | always        |
| `gg_ann`        | `Feed`, huge font  | center, `y 0.30`                | —                                  | level, last level, lost level, lead | always        |

Params and colors:

| Key         | Params, colors and texts                                                                               |
|-------------|--------------------------------------------------------------------------------------------------------|
| `gg_place`  | `fade_ms` 300, `warn_at` 3, `warn_pulse_ms` 500; `flash_ms` 600 in the match, 0 otherwise              |
| `gg_box`    | color `0 0 0 110`, `fade_ms` 300, `accent_wide` 2, `radius` 3                                          |
| `gg_rival`  | `fade_ms` 300; slot 1 `255 106 77`, slot 2 `255 210 74`, slot 3 `124 214 124`                          |
| `gg_kills`  | `fade_ms` 300                                                                                          |
| `gg_levels` | `fade_ms` 300, `track` 1                                                                               |
| `gg_top`    | `fade_ms` 300, `board_mode` 1; variant 0 `{@place}.⇥{@name:16}⇥{level:cur}` for everybody (⇥ is a TAB) |
| `gg_feed`   | flags `NoScoreboard` only, `life_ms` 6000                                                              |
| `gg_ann`    | `lines` 1, `life_ms` 2500, `line_fade_ms` 300, `queue` 1                                               |

- "Match" means the plugin shows the element while `phase` is 0, "until the win" while `phase` is
  not 3.
- `phase` 3 has no variant in `gg_place`, so after the win the place block is empty.
- `gg_box` is a dark plate with an accent bar in `hud_color` behind `gg_place`, `gg_rival` and
  `gg_kills`, the elements whose parent chain reaches `gg_place`. In the warmup it is behind
  `gg_place` alone, and it fades with them.
- `gg_place` warns in the last 3 seconds of the warmup and the countdown: red and pulsing. The
  plugin sets `phase_left` only while `phase` is 1 or 2 and clears it otherwise, so the warning
  never holds in the match or after the win. `flash_ms` is 600 only in the match, where the text
  changes with the place; the warmup texts change every second.
- `gg_rival` is colored by its variant: overtake orange-red, tied yellow, lead green; waiting keeps
  `hud_color`.
- `gg_levels` shows the other players above the levels they are on (`track`, 8.8).
- `gg_top` lists the first `rows` (3 by default) players, or teams in teamplay, with their place,
  name, weapon icon (the row's `weapon`) and level; a viewer whose player or team is not among them
  sees its row after `…`. Its template has no words, so it is set once for everybody.
- `gg_feed` has no `Subject`, so a roaming spectator sees it too. Both feeds get lines only through
  events (below).
- The banner rows read the leader's `level`: `left` is 0 only while the leader is on the last
  level, and only variant 0 has a template, so otherwise the rows are empty. The client decides
  this with the same leader it shows, and the plugin sends nothing when the leader changes.
- After the win the winner stays on the last level with all its kills, and the bottom row would
  say `0 kills to win`, so the plugin hides both banner rows in `phase` 3.
- The key of the bottom row is `gg_banner_bot`: `gg_banner_bottom` exceeds the 15-byte limit.
- The `Meter` counter is the default `16 / 25`.
- The default params are the sizes and alphas of the GunGame look.

```pawn
#define HUD_TOP 0.025 // 12 of 480 units

Scoreboard_Setup(){
	if(!bhlApi)
		return
	
	bhl_sb_reset()
	bhl_sb_config(BHL_SB_HIDE_FRAGS | BHL_SB_HIDE_DEATHS | BHL_SB_HIDE_EFF | BHL_SB_HIDE_STEAMID,"",get_pcvar_num(cvar[CVAR_TEAMPLAY]) == 1 ? "TEAM" : "FFA")
	
	bhl_sb_add_column("rank","",BHL_SB_BUILTIN)
	Scoreboard_SetColumns(warmUpMode == 1 || warmUpMode == 3)
	bhl_sb_add_badge("final","weapon")
	bhl_sb_add_column("phase","",BHL_SB_TEXT,0,0,BHL_SB_COL_HIDDEN)
	bhl_sb_add_column("phase_left","",BHL_SB_TEXT,0,0,BHL_SB_COL_HIDDEN)
	
	bhl_sb_list_resize("levels",maxLevel_Int)
	sbPhase = -1
	hudLeaderUserId = 0
	
	new weaponSet[weaponSetStruct]
	
	for(new i ; i < maxLevel_Int ; ++i){
		ArrayGetArray(weaponSets_Array,i,weaponSet)
		bhl_sb_list_set_icon("levels",i,weaponSet[WSET_ICONSPRITE],weaponSet[WSET_SHOWNAME])
	}
	
	Hud_Setup()
	Scoreboard_SetPhase(0)
}

Scoreboard_SetColumns(bool:warmup){
	new bool:teamplay = get_pcvar_bool(cvar[CVAR_TEAMPLAY])
	new BhlSbColumnFlags:levelFlags = warmup ? BHL_SB_COL_HIDDEN : BHL_SB_COL_NONE
	new BhlSbColumnFlags:killsFlags = warmup ? BHL_SB_COL_PRIMARY : (teamplay ? BHL_SB_COL_NONE : BHL_SB_COL_HIDDEN)
	
	sbWarmupColumns = warmup
	
	bhl_sb_add_column("level","#BHL_SB_Level",BHL_SB_STEPS,60,0,BHL_SB_COL_MARK_LAST | (warmup ? BHL_SB_COL_HIDDEN : BHL_SB_COL_PRIMARY))
	bhl_sb_add_column("weapon","#BHL_SB_Weapon",BHL_SB_ICON,100,0,levelFlags)
	bhl_sb_add_column("progress","#BHL_SB_Progress",BHL_SB_METER,teamplay ? 60 : 90,0,levelFlags)
	bhl_sb_add_column("kills","#BHL_SB_Kills",BHL_SB_TEXT,60,warmup ? 1 : 2,BHL_SB_COL_RIGHT | killsFlags)
	bhl_sb_add_column("score","",BHL_SB_TEXT,0,warmup ? 2 : 1,BHL_SB_COL_HIDDEN)
}

Hud_Setup(){
	bhl_hud_hide_std(BHL_HUD_STD_TIMER | BHL_HUD_STD_NEXTMAP | BHL_HUD_STD_CUSTOM_TIMERS)
	
	bhl_hud_add("gg_place",BHL_HUD_TEXT,-1.0,HUD_TOP,BHL_HUD_FONT_LARGE)
	bhl_hud_bind("gg_place","phase_left",.select = "phase")
	bhl_hud_param("gg_place",BHL_HUD_P_FADE_MS,300)
	bhl_hud_param("gg_place",BHL_HUD_P_WARN_AT,3)
	bhl_hud_param("gg_place",BHL_HUD_P_WARN_PULSE_MS,500)
	
	bhl_hud_add("gg_box",BHL_HUD_BOX)
	bhl_hud_place("gg_box",BHL_HUD_PARENT,"gg_place",6)
	bhl_hud_color("gg_box",0,0,0,110)
	bhl_hud_param("gg_box",BHL_HUD_P_FADE_MS,300)
	bhl_hud_param("gg_box",BHL_HUD_P_ACCENT_WIDE,2)
	bhl_hud_param("gg_box",BHL_HUD_P_RADIUS,3)
	
	bhl_hud_add("gg_rival",BHL_HUD_TEXT)
	bhl_hud_place("gg_rival",BHL_HUD_PARENT,"gg_place",2)
	bhl_hud_bind("gg_rival",.select = "@state")
	bhl_hud_param("gg_rival",BHL_HUD_P_FADE_MS,300)
	bhl_hud_slot_color("gg_rival",1,255,106,77)
	bhl_hud_slot_color("gg_rival",2,255,210,74)
	bhl_hud_slot_color("gg_rival",3,124,214,124)
	
	bhl_hud_add("gg_kills",BHL_HUD_METER)
	bhl_hud_place("gg_kills",BHL_HUD_PARENT,"gg_rival",6)
	bhl_hud_bind("gg_kills","progress")
	bhl_hud_param("gg_kills",BHL_HUD_P_FADE_MS,300)
	
	bhl_hud_add("gg_levels",BHL_HUD_STRIP)
	bhl_hud_place("gg_levels",BHL_HUD_NUMBERS_ROW,"",3)
	bhl_hud_bind("gg_levels","level","levels")
	bhl_hud_param("gg_levels",BHL_HUD_P_FADE_MS,300)
	bhl_hud_param("gg_levels",BHL_HUD_P_TRACK,1)
	
	// Rows are teams in teamplay
	bhl_hud_add("gg_top",BHL_HUD_BOARD,0.015,HUD_TOP,BHL_HUD_FONT_TEXT,BHL_HUD_LEFT | BHL_HUD_TOP)
	bhl_hud_bind("gg_top","weapon")
	bhl_hud_param("gg_top",BHL_HUD_P_FADE_MS,300)
	bhl_hud_param("gg_top",BHL_HUD_P_BOARD_MODE,1)
	bhl_hud_text(0,"gg_top",0,"{@place}.^t{@name:16}^t{level:cur}")
	
	bhl_hud_add("gg_banner_top",BHL_HUD_TEXT,.flags = BHL_HUD_LEADER)
	bhl_hud_place("gg_banner_top",BHL_HUD_BANNER_TOP)
	bhl_hud_bind("gg_banner_top",.select = "level:left")
	
	bhl_hud_add("gg_banner_bot",BHL_HUD_TEXT,.flags = BHL_HUD_LEADER)
	bhl_hud_place("gg_banner_bot",BHL_HUD_BANNER_BOTTOM)
	bhl_hud_bind("gg_banner_bot",.select = "level:left")
	
	bhl_hud_add("gg_feed",BHL_HUD_FEED,-0.015,0.22,BHL_HUD_FONT_SMALL,BHL_HUD_RIGHT | BHL_HUD_TOP,BHL_HUD_NO_SCOREBOARD)
	bhl_hud_param("gg_feed",BHL_HUD_P_LIFE_MS,6000)
	
	bhl_hud_add("gg_ann",BHL_HUD_FEED,-1.0,0.30,BHL_HUD_FONT_HUGE)
	bhl_hud_param("gg_ann",BHL_HUD_P_LINES,1)
	bhl_hud_param("gg_ann",BHL_HUD_P_LIFE_MS,2500)
	bhl_hud_param("gg_ann",BHL_HUD_P_LINE_FADE_MS,300)
	bhl_hud_param("gg_ann",BHL_HUD_P_QUEUE,1)
	
	Hud_SetTexts(0)
}

//
// Texts in the language of the player, 0 for everybody.
// Also called from client_putinserver and when the player changes "lang".
//
Hud_SetTexts(id){
	if(!bhlApi || !ggActive)
		return
	
	bhl_hud_text_lang(id,"gg_place",0,"GG_HUD_PLACE")
	bhl_hud_text_lang(id,"gg_place",1,"GG_HUD_WARMUP")
	bhl_hud_text_lang(id,"gg_place",2,"GG_HUD_START_IN")
	bhl_hud_text_lang(id,"gg_rival",0,"GG_HUD_WAITING")
	bhl_hud_text_lang(id,"gg_rival",1,"GG_HUD_OVERTAKE")
	bhl_hud_text_lang(id,"gg_rival",2,"GG_HUD_TIED")
	bhl_hud_text_lang(id,"gg_rival",3,"GG_HUD_LEAD")
	bhl_hud_text_lang(id,"gg_banner_top",0,"GG_SB_BANNER_TITLE")
	bhl_hud_text_lang(id,"gg_banner_bot",0,"GG_SB_BANNER_LINE")
	bhl_hud_text_lang(id,"gg_feed",0,"GG_HUD_FEED_LAST")
	bhl_hud_text_lang(id,"gg_feed",1,"GG_HUD_FEED_DOWN")
	bhl_hud_text_lang(id,"gg_feed",2,"GG_HUD_FEED_LEAD")
	bhl_hud_text_lang(id,"gg_ann",0,"GG_HUD_ANN_LEVEL")
	bhl_hud_text_lang(id,"gg_ann",1,"GG_HUD_ANN_LAST")
	bhl_hud_text_lang(id,"gg_ann",2,"GG_HUD_ANN_DOWN")
	bhl_hud_text_lang(id,"gg_ann",3,"GG_HUD_ANN_LEAD")
}

Scoreboard_SetPhase(timeLeft){
	if(!bhlApi)
		return
	
	new phase
	
	if(isEndGame)
		phase = 3
	else if(warmUpMode == 1)
		phase = 1
	else if(warmUpMode == 3)
		phase = 2
	
	new bool:warmup = phase == 1 || phase == 2
	
	if(warmup != sbWarmupColumns)
		Scoreboard_SetColumns(warmup)
	
	bhl_sb_set_int(BHL_SB_GLOBAL,"phase",phase)
	
	// Missing outside the countdowns, so the warning of gg_place holds only there
	if(warmup)
		bhl_sb_set_int(BHL_SB_GLOBAL,"phase_left",timeLeft)
	else
		bhl_sb_clear(BHL_SB_GLOBAL,"phase_left")
	
	//
	// A write for player 0 drops per-player visibility, so only on a phase change
	//
	if(phase == sbPhase)
		return
	
	sbPhase = phase
	
	new bool:match = phase == 0
	new bool:won = phase == 3
	
	bhl_hud_show(0,"gg_rival",match)
	bhl_hud_show(0,"gg_kills",match)
	bhl_hud_show(0,"gg_levels",match)
	bhl_hud_show(0,"gg_top",match)
	
	// Warmup texts change every second
	bhl_hud_param("gg_place",BHL_HUD_P_FLASH_MS,match ? 600 : 0)
	
	// After the win the leader line would read "0 kills to win"
	bhl_hud_show(0,"gg_banner_top",!won)
	bhl_hud_show(0,"gg_banner_bot",!won)
}

bool:Player_HasBhlHud(id){
	return bhlApi && (bhl_get_client_caps(id) & BHL_CAPS_HUD_V1) != BHL_CAPS_NONE
}

//
// The event is about the player himself, or about his team in teamplay
//
bool:Hud_IsOwnEvent(player,id){
	return player == id || get_pcvar_bool(cvar[CVAR_TEAMPLAY]) && isPlayersTeamSame(player,id)
}

//
// Level change from gg_notify_msg (NOTIFY_UP to NOTIFY_SKIP), also when its chat line is off:
// the announcer of the player and his team, the feed of the others on the last level or a lost level.
// In teamplay once per team level, as the chat: after a level down the equip of each teammate notifies it.
//
Hud_LevelEvent(id,notifyId){
	if(!bhlApi || notifyId > NOTIFY_SKIP)
		return
	
	new level = playersData[id][PLAYER_CURRENTLEVEL]
	
	if(get_pcvar_bool(cvar[CVAR_TEAMPLAY])){
		static lastTeam,lastLevel
		
		if(lastTeam == playersData[id][PLAYER_TEAM] && lastLevel == level)
			return
		
		lastTeam = playersData[id][PLAYER_TEAM]
		lastLevel = level
	}
	
	new weaponSet[weaponSetStruct]
	ArrayGetArray(playersData[id][PLAYER_WEAPONSETS],level,weaponSet)
	
	new bool:down = notifyId == NOTIFY_DOWN
	new bool:last = isPlayerLastLevel(id)
	new players[32],pnum
	get_players(players,pnum,"ch")
	
	for(new i,player ; i < pnum ; ++i){
		player = players[i]
		
		if(!Player_HasBhlHud(player))
			continue
		
		if(Hud_IsOwnEvent(player,id))
			bhl_hud_event(player,"gg_ann",down ? 2 : _:last,0,weaponSet[WSET_ICONSPRITE],weaponSet[WSET_SHOWNAME],level + 1)
		else if(down)
			bhl_hud_event(player,"gg_feed",1,0,"","",id)
		else if(last)
			bhl_hud_event(player,"gg_feed",0,0,weaponSet[WSET_ICONSPRITE],weaponSet[WSET_SHOWNAME],id)
	}
	
	Hud_CheckLeader()
}

//
// The announcer or the feed of a player with the HUD shows this chat line: his own level change,
// the last level or a lost level of the others. The skip line keeps its reason.
//
bool:Hud_ShowsNotify(player,id,notifyId){
	switch(notifyId){
		case NOTIFY_UP,NOTIFY_LAST,NOTIFY_TEAMUP,NOTIFY_TEAMLAST:{
			if(!Hud_IsOwnEvent(player,id) && !isPlayerLastLevel(id))
				return false
		}
		case NOTIFY_DOWN:{}
		default:
			return false
	}
	
	return Player_HasBhlHud(player)
}
```

`Scoreboard_SetLevelCells` fills the level cells and the badge of its target: the player without
teamplay, the team in teamplay. A level change that waits for the respawn equip already shows the
kills the equip sets: 0 after a level-up, one short of the next level (`WSET_KILLS − 1`) after a
level-down. For the winner `level` stays on the last level:

```pawn
	if(level == maxLevel - 1)
		bhl_sb_set_text(target,"final","#BHL_SB_Final")
	else
		bhl_sb_clear(target,"final")
```

Events. `gg_notify_msg` calls `Hud_LevelEvent(id, notifyId)` first, so the events go out also
when the chat line of that notification is turned off. Each player in game with `HudV1` (bots and
HLTV skipped) gets one event, and level changes of other players below the last level give none.
In teamplay a team level sends its events once, like its chat line, though the equip of every
teammate notifies a level down:

| Notification of player `id`             | Player `id`, his team in teamplay      | Other players                             |
|-----------------------------------------|----------------------------------------|-------------------------------------------|
| Level up or skip, not to the last level | `gg_ann` 0 `LEVEL n · weapon`          | —                                         |
| Level up or skip to the last level      | `gg_ann` 1 `LAST LEVEL · weapon`       | `gg_feed` 0 `name reached the last level` |
| Level down                              | `gg_ann` 2 `LEVEL LOST · weapon`       | `gg_feed` 1 `name lost a level`           |
| New unique leader, without teamplay     | `gg_ann` 3 `YOU TAKE THE LEAD`, to him | `gg_feed` 2 `name takes the lead`         |

- The announcer events carry the weapon of the new level as their icon (`WSET_ICONSPRITE`, its
  `WSET_SHOWNAME` as `@icon`) and the level, 1-based, as `@a`. The feed events carry the player
  index as `@a`, written with `{@a:name}`, and the "last level" line the weapon icon too.
- After every level event, kill and descore `Hud_CheckLeader()` looks for the leader: the only
  participant with the best score P (`Scoreboard_SetLevelCells` with `scoreOnly`, which also sends
  `score` when a skip raised it). A tie keeps the last leader. When the leader changes (by userid,
  as slots are reused; `Scoreboard_Setup` forgets it) and at least 2 participants play, the leader
  gets `gg_ann` 3 and the others `gg_feed` 2. Teams share the level in teamplay, so there is no
  leader event there.
- `life_ms` 0 in all events: the lines live for the `life_ms` of their feed.

Chat. The plugin doesn't print a chat notification to a player with `HudV1` when his feed or
announcer shows it (`Hud_ShowsNotify`): his own (or his team's) level up and last level, the last
level of any other player, and any level down. Level-ups of others below the last level, the skip
line with its reason, the win and the mode messages stay in chat for everybody. A player without
`HudV1` gets every chat line as before.

`gungame.txt` gets the texts in English, Russian and Spanish. Other languages follow the same
pattern; a language with two forms uses two. The texts go through `%L`, so a literal `%` in them
is written `%%`.

```
[en]
GG_HUD_PLACE = PLACE {@place} / {@total}
GG_HUD_WARMUP = WARMUP {@value:time}
GG_HUD_START_IN = START IN {@value}
GG_HUD_WAITING = Waiting for opponents
GG_HUD_OVERTAKE = To pass {@rival:20}: {@pass|# kill|# kills}
GG_HUD_TIED = Tied
GG_HUD_LEAD = Lead: {@lead|# kill|# kills}
GG_SB_BANNER_TITLE = Leader on the last level
GG_SB_BANNER_LINE = {@name} · {weapon} · {progress:left|# kill|# kills} to win
GG_HUD_FEED_LAST = {@a:name} reached the last level
GG_HUD_FEED_DOWN = {@a:name} lost a level
GG_HUD_FEED_LEAD = {@a:name} takes the lead
GG_HUD_ANN_LEVEL = LEVEL {@a} · {@icon}
GG_HUD_ANN_LAST = LAST LEVEL · {@icon}
GG_HUD_ANN_DOWN = LEVEL LOST · {@icon}
GG_HUD_ANN_LEAD = YOU TAKE THE LEAD

[ru]
GG_HUD_PLACE = МЕСТО {@place} / {@total}
GG_HUD_WARMUP = РАЗМИНКА {@value:time}
GG_HUD_START_IN = СТАРТ ЧЕРЕЗ {@value}
GG_HUD_WAITING = Ожидание соперников
GG_HUD_OVERTAKE = До обгона {@rival:20}: {@pass|# убийство|# убийства|# убийств}
GG_HUD_TIED = Вровень
GG_HUD_LEAD = Отрыв: {@lead|# убийство|# убийства|# убийств}
GG_SB_BANNER_TITLE = Лидер на последнем уровне
GG_SB_BANNER_LINE = {@name} · {weapon} · {progress:left|# убийство|# убийства|# убийств} до победы
GG_HUD_FEED_LAST = {@a:name} выходит на последний уровень
GG_HUD_FEED_DOWN = {@a:name} теряет уровень
GG_HUD_FEED_LEAD = {@a:name} выходит в лидеры
GG_HUD_ANN_LEVEL = УРОВЕНЬ {@a} · {@icon}
GG_HUD_ANN_LAST = ПОСЛЕДНИЙ УРОВЕНЬ · {@icon}
GG_HUD_ANN_DOWN = УРОВЕНЬ ПОТЕРЯН · {@icon}
GG_HUD_ANN_LEAD = ВЫ ВЫШЛИ В ЛИДЕРЫ
```

The longest text, the Russian banner line, takes 111 bytes. A plugin that doesn't want to send
texts to every player can use the client tokens instead (10.1), in English and Russian only:
`bhl_hud_text(0,"gg_place",0,"#BHL_HUD_Place")`. The `gg_top` row has no words and is set once
with `bhl_hud_text(0, …)`.

| Plugin function          | Calls                                                                            |
|--------------------------|----------------------------------------------------------------------------------|
| `Enable_GunGame`         | `Scoreboard_Setup()`                                                             |
| `Disable_GunGame`        | `bhl_sb_reset()`; the standard HUD parts come back                               |
| `client_putinserver`     | `Scoreboard_UpdatePlayer(id)`; `Hud_SetTexts(id)` except for bots and HLTV       |
| `client_infochanged`     | `Hud_SetTexts(id)` when `lang` changed, only for a connected player              |
| `Equip_PlayerWithWeapon` | `Scoreboard_UpdatePlayer(id)`                                                    |
| Descore (suicide)        | `Scoreboard_UpdatePlayer(victim)`, `Hud_CheckLeader()`                           |
| Kill and level-up        | `Scoreboard_UpdatePlayer(killer)`, team cells in teamplay, `Hud_CheckLeader()`   |
| `Update_TeamData`        | Team cells through `bhl_sb_team(teamName)`                                       |
| `WarmUp_Timer`           | `Scoreboard_SetPhase(left)` every second                                         |
| Match start              | `Scoreboard_SetPhase(0)`                                                         |
| `resetPlayersScore`      | `kills` 0 for everyone                                                           |
| Disconnect               | Team `kills` without the player                                                  |
| `endgame`                | Final player and team cells; `Scoreboard_SetPhase(0)` sets phase 3 (`isEndGame`) |
| Auto-save restore        | `Scoreboard_UpdatePlayer(id)`                                                    |
| `gg_notify_msg`          | `Hud_LevelEvent(id, notifyId)`, then chat lines filtered by `Hud_ShowsNotify`    |

The engine calls `client_infochanged` before `ClientConnect` too, when no text can be set for the
slot (9.2). So the plugin keeps the `lang` a player's texts were set for: it stores it in
`client_putinserver` after `Hud_SetTexts(id)`, compares it in `client_infochanged` and clears it on
disconnect.

For players with `HudV1` (`Player_HasBhlHud`) the plugin skips:

- the text informer (`Show_HudInformer`);
- `StatusIcon`;
- the warmup messages from `WarmUp_Timer`, which then go to every other player one by one
  instead of `show_hudmessage(0, …)`;
- the chat lines of level changes that his feed or announcer shows (see Chat above).

`bhl_client_caps_changed` switches the informer and `StatusIcon` on and off when a player changes
`hud_server_elements`; the other checks are made for every message. The winner message, the start
message (`WARMUP_ROUND_OVER`), the final table and the other chat notifications stay for everyone.

A plugin with a long countdown can send a `Clock` (4.3) once instead of an `Int` every second.
GunGame keeps the `Int`: its warmup timer runs every second anyway.

## 10. Localization

### 10.1 Tokens

The client ships these tokens in `bugfixedhl_english.utf8.txt` and `bugfixedhl_russian.utf8.txt`.
Other client languages use English.

| Token              | English                                         | Russian                                                             |
|--------------------|-------------------------------------------------|---------------------------------------------------------------------|
| `BHL_SB_Level`     | `Level`                                         | `Уровень`                                                           |
| `BHL_SB_Weapon`    | `Weapon`                                        | `Оружие`                                                            |
| `BHL_SB_Progress`  | `Progress`                                      | `Прогресс`                                                          |
| `BHL_SB_Kills`     | `Kills`                                         | `Убийства`                                                          |
| `BHL_SB_Final`     | `FINAL`                                         | `ФИНАЛ`                                                             |
| `BHL_HUD_Place`    | `PLACE {@place} / {@total}`                     | `МЕСТО {@place} / {@total}`                                         |
| `BHL_HUD_Overtake` | `To pass {@rival:20}: {@pass\|# kill\|# kills}` | `До обгона {@rival:20}: {@pass\|# убийство\|# убийства\|# убийств}` |
| `BHL_HUD_Lead`     | `Lead: {@lead\|# kill\|# kills}`                | `Отрыв: {@lead\|# убийство\|# убийства\|# убийств}`                 |
| `BHL_HUD_Tied`     | `Tied`                                          | `Вровень`                                                           |
| `BHL_HUD_Waiting`  | `Waiting for opponents`                         | `Ожидание соперников`                                               |
| `BHL_HUD_Warmup`   | `WARMUP {@value:time}`                          | `РАЗМИНКА {@value:time}`                                            |
| `BHL_HUD_StartIn`  | `START IN {@value}`                             | `СТАРТ ЧЕРЕЗ {@value}`                                              |

- `BHL_HUD_*` tokens are templates that use only built-in values, so any ranked mode can use them;
  without sort keys they rank by frags (6.4). `BHL_HUD_Warmup` and `BHL_HUD_StartIn` read the
  element's `value`.
- Texts of one mode, such as the GunGame banner, are plugin texts. A plugin covers any language by
  sending each player a template in his language (`bhl_hud_text_lang`, 9.2).
- Badge cells are broadcast to everybody, so a badge in the player's language needs a client
  token.
- Player names are never looked up as tokens. A team name that starts with `#` is localized when
  VGUI2 has it, as in the team header (8.10).

### 10.2 Word forms

The number of forms in `{n|…}` selects the rule, so a template in any language picks its rule
without the client knowing the language:

| Forms | Form 1                          | Form 2                                  | Form 3 |
|-------|---------------------------------|-----------------------------------------|--------|
| 1     | Always                          | —                                       | —      |
| 2     | `n = 1`                         | Other                                   | —      |
| 3     | `n % 10 = 1` and `n % 100 ≠ 11` | `n % 10` in 2–4 and `n % 100` not 12–14 | Other  |

- `n` is the absolute value of the number. Forms after the third are ignored.
- Two forms fit English, Spanish, German and similar languages; three forms fit Russian,
  Ukrainian and Belarusian. Languages with other rules (Polish, Czech) pick the closer one.

### 10.3 Weapon names

Names of standard HL weapons are the same in every language, as in the mockup. They are tokens
`BHL_Weapon_<sprite without d_>` with English text in every language file. An `Icon` value with a
`d_*` sprite shows this name; for other sprites `Icon.text` from the server is used.

| Sprite         | Name     |
|----------------|----------|
| `d_crowbar`    | Crowbar  |
| `d_9mmhandgun` | Glock    |
| `d_357`        | .357     |
| `d_9mmAR`      | MP5      |
| `d_shotgun`    | Shotgun  |
| `d_crossbow`   | Crossbow |
| `d_bolt`       | Crossbow |
| `d_rpg_rocket` | RPG      |
| `d_gauss`      | Gauss    |
| `d_egon`       | Egon     |
| `d_hornet`     | Hornet   |
| `d_grenade`    | Grenade  |
| `d_satchel`    | Satchel  |
| `d_tripmine`   | Tripmine |
| `d_snark`      | Snark    |

## 11. Compatibility

Client and server:

| Client \ Server                     | Old / AG / Valve | New, inactive | New, scoreboard (`Config`) | New, HUD (elements)      |
|-------------------------------------|------------------|---------------|----------------------------|--------------------------|
| Old (BugfixedHL, Valve, AG, OpenAG) | as now           | as now        | as now                     | as now; plugin HUD texts |
| New, defaults                       | standard         | standard      | custom                     | elements                 |
| New, `hud_scoreboard_custom 0`      | standard         | standard      | standard                   | elements                 |
| New, `hud_server_elements 0`        | standard         | standard      | custom                     | plugin HUD texts         |
| New, both 0                         | standard         | standard      | standard                   | plugin HUD texts         |
| New, `_bhl` not in userinfo         | standard         | standard      | custom via query ¹         | elements via query ¹     |

The last two columns are independent: a server that sends a `Config` and elements gives the
scoreboard from the "New, scoreboard" column and the HUD from the "New, HUD" column.

¹ If the client passes the p47 and LAN checks. A changed client setting reaches the server within
10 s (3.3).

GunGame on a server with the new plugin:

| Client                         | Scoreboard                     | HUD                                                                                                         |
|--------------------------------|--------------------------------|-------------------------------------------------------------------------------------------------------------|
| Old                            | standard                       | Plugin informer, `StatusIcon`, warmup texts, all chat lines                                                 |
| New, defaults                  | GunGame columns, banner, badge | Elements with feed and announcer; plugin texts off except winner, start and chat lines the feeds don't show |
| New, `hud_server_elements 0`   | GunGame columns, banner, badge | Plugin informer, `StatusIcon`, warmup texts, all chat lines; timer shown                                    |
| New, `hud_scoreboard_custom 0` | standard                       | As with the defaults                                                                                        |
| New, both 0                    | standard                       | Plugin informer, `StatusIcon`, warmup texts, all chat lines; timer shown                                    |

On a client that draws the elements, GunGame hides the BugfixedHL timer, the next map line and
custom timers (8.12). The bottom speedometer and `hud_jumpspeed` stay and can cross the level
strip.

Module and server DLL:

| `bugfixedapi_amxx` \ `hl.so` | API 1.0–1.3                                                             | API 1.4    |
|------------------------------|-------------------------------------------------------------------------|------------|
| Old module                   | as now                                                                  | as now     |
| New module                   | old natives work, no `bhl_sb_*`, `bhl_hud_*`, `bhl_get_client_caps()` 0 | everything |
| No module                    | GunGame works as now                                                    | same       |

Draft 3 changes nothing for old clients, Valve, AG and OpenAG clients, old servers and existing
plugins: everything new travels in `BhlScore`, which only `ScoreboardV1` clients get, as a new
opcode, new kinds, params from id 32 and an optional block at the end of `Element`. An element
that uses none of it is byte-identical to draft 2, and the old natives keep their behaviour; the
new methods and natives are appended (9.1, 9.2).

Draft test builds are not supported: the client, `hl.so`, the module and the plugin are updated
together. In draft 1 builds `_bhl` bit 1 and `Config` mean something else, and their `hl.so`
reports API 1.2. Draft 2 builds report API 1.3 without the methods of 1.4, cap the params of an
`Element` at 16 and know none of the new kinds, params and messages. The new module refuses both
(9.1).

## 12. Test plan

Capabilities:

- From a second client, the first client's userinfo has no `_bhl` (keys with `_` are not forwarded).
- Fill the userinfo with `setinfo` keys: `_bhl` doesn't fit, `bhl_caps` shows source `Query`, the
  custom scoreboard and the elements still work.
- `setinfo` during a game reaches `ClientUserInfoChanged`. `hud_scoreboard_custom 0/1` and
  `hud_server_elements 0/1`, alone and together, switch the scoreboard, the elements and the plugin
  texts both ways; only switching both off and on brings a full sync.
- Writing the same `_bhl` value again doesn't cause `ClientUserInfoChanged` spam.
- With source `Query`, `hud_server_elements 0/1` reaches the plugin within 10 s.
- `setinfo _bhl 2` and a bot with `_bhl 3`: `bhl_get_client_caps()` returns 0.
- Both cvars to 0, then one back to 1: no old data is drawn before the full sync.

Protocol and server:

- Every cell of the compatibility tables in section 11.
- Map change, reconnect, joining mid-game, GunGame on and off, teamplay.
- 32 players with bots: the full sync with the HUD part never triggers `Reliable channel overflowed`.
- The full sync arrives after the client's `VidInit` and nothing is lost; elements appear only
  after their texts.
- A plugin with elements and hidden columns but no `Config`: the standard scoreboard and the
  elements.
- Adding, changing, removing and removing all elements; ids reused after a removal; a removal and
  a new element in one frame and during the 0.1 s pause, also with every other id taken (no old
  variant is drawn); a kind change and back in one frame.
- A plugin that changes 32 cells every frame at `sys_ticrate 1000` never causes `Reliable channel
  overflowed`.
- A parent defined after its child; a parent removed (the children disappear) and added again; a
  parent id given to an unrelated element; a parent cycle; a banner element as a parent.
- Two plugins hiding different standard parts; a player-0 visibility write that doesn't change the
  default drops the per-player overrides.
- Per-player texts and visibility: an English and a Russian player on one server, a spectator in
  `OBS_IN_EYE` of a player with the other language, a player joining later, a reconnect into the
  same slot.
- Malformed messages: short `Element`, `Template` and `HudState`, strings without a NUL, more params
  than `count`, unknown kinds, places, fonts and params, templates for undefined elements. The
  client doesn't crash (checked offline by feeding the parser).
- Layout without data: a `Strip` without a list and without a value, a `Meter` without a value, an
  `Icon` without a value, each as a parent; `cell_min_wide` and `cell_gap` at their minimum at
  640×480. No division by zero.
- A template of 188 bytes is set; one of 189 bytes is refused with an error.
- An element that uses nothing of draft 3 is byte-identical to the draft 2 message. A `Strip` with
  all 17 params and all 10 color slots (181 bytes) is read whole; a slot of 10 or more and params
  of other kinds are skipped and the slots after them still apply.
- A param of every kind on each kind; a param of another kind is refused. `bhl_hud_slot_color()`
  with `r = -1` clears the slot, and the element is sent without the color block again.
- Events: to player 0 and to one player; a client without `HudV1`, or not ready yet, gets nothing,
  also later; an event during the full sync of a client waits and arrives after its `HudState`; an
  event for an element removed before it was sent waits and is dropped after its life; 9 events in
  one frame give the newest 8 in order; a map change, `bhl_sb_reset()` and a reconnect into the slot
  clear the queue; a full sync doesn't replay events. A non-feed key, variant 8, 5 numbers and a
  player who is not connected return false.
- Malformed `Event` messages: a short header, `count` larger than the args, an unknown arg type, an
  event for an element that is not a `Feed` or has no template for the variant.
- The new module with a draft 2 `hl.so` (API 1.3): `bhl_sb_available()` is false,
  `bhl_get_client_caps()` 0, the other natives log one error and do nothing.

Templates (`hud_server_eval` and an offline table run through the parser):

- Word forms for 0, 1, 2, 5, 11, 12, 21, 22, 25 and −1 with 1, 2 and 3 forms, with and without `#`.
- `time` for −5, 0, 15, 75, 3600; `time2` for an `Int` and for running, stopped and counting-down
  clocks; cuts in Latin and Cyrillic, `…` only when cut; `{{ }}`; a lone `}`; `{ref?}`; missing
  refs.
- Literal: `{}`, `{ @place }`, `{Имя}`, an unclosed `{`, `#1`. Missing: `{x:foo}`, `{@foo}`. Parse
  errors: `{x:cur:max}`, `{x:time|a|b}`, `{x:20:30}`, `{x:0}`, 17 placeholders.
- A client token that is missing; a token whose text is `#Other` is not looked up again.
- `^` refs and `@behind`, `@score`, `@frags`, `@deaths` with and without sort keys, in team mode and
  without participants.
- A player named `{@place}` stays literal in `@rival` and in `Text` cells; one named
  `#BHL_SB_Final` stays literal in `@name`, `@rival` and the `name` format.
- An element whose `value` is `@value`, `^@value` or `@value:cur`: the value is missing, without
  recursion.
- `@index`, `@item` and `@value` in ranking and list boards; `@a`–`@d`, `{@a:name}`, `@icon` and an
  arg of an unknown type in events; all of them and `{^@a}` are missing in a `Text` element and in
  `hud_server_eval`.
- A `Board` template with TABs: 1, 4 and 5 columns.

HUD elements of draft 3:

- `Board`: ranking and list mode; `board_mode` 0, 1 and 2 with the own row first, last, in the
  middle and outside the first rows; the separator; team mode; a roaming spectator has no own row;
  numbers right-aligned and names left-aligned; icons of different widths and a missing sprite;
  fill with `Progress` and `Int` values, 0 and negative ones; the own row with and without
  variant 1.
- `Feed`: `queue` 0 with more lines than `lines`, lines expiring and fading; `queue` 1 with 3 events
  in one frame shown one after another; more than 32 lines over two feeds; a removal and a `Reset`
  drop the lines; top and bottom anchors, left, center and right alignment; a line with only an
  icon, and one whose template has a missing value and no icon (dropped).
- `Box`: around a parent with children, a `Meter` counter and `Strip` scroll and track markers;
  nothing drawn in the group hides it; a `Box` without a parent place and an element placed at a
  `Box` are unplaced; a radius above half the height; the accent bar with and without slot 9; it
  fades with its group.
- Effects: a flash on a content change only, none on appearance, a spectator switching players, a
  full sync or a reconnect; `hold_ms` hides the element, and a change shows it again; a fade in,
  out and reversed halfway; `hud_draw 0`, an intermission, `hud_server_elements 0`, a removal and a
  kind change hide at once; `warn_at` with an `Int`, a `Progress` and a running `Clock`; the pulse;
  the color order with `HudColor`, a variant slot, the warning and a flash.
- Ring: `max ≤ dots`, `max > dots`, `dots` 0 with `max` 10 (one dot per unit, no counter with
  `counter` 1), `counter` 2; `dot_size` 2 and 64.
- Track: 1, 2 and 3 or more players on one cell, positions out of range, the subject left out, a
  team subject with `Leader`, no markers over hidden cells.
- Sprite files: a precached file in a `Strip` list, an `Icon` element, a `Board`, a `Feed` line and
  a scoreboard `Icon` column, with OpenGL and the software renderer; a missing file and a broken one
  (wrong header, a frame above 1024) draw only the text and don't stop the engine, also with
  `developer 2`; 33 different paths; a map change loads the files again.
- With OpenGL a `Box` and a `Board` plate drawn after a sprite of a lower id are translucent.

GunGame scoreboard:

- 640×480, 1280×720 and 1920×1080: the table has the size of the standard one and the columns fit.
- 12 and 32 players: compact spacing, then scrolling.
- 9, 14 and 24 levels in the level strip with the last step marked; dots without a counter with
  1–10 kills, the bar with the counter with more.
- Warmup: level, weapon and progress are hidden, kills and places by kills are shown; the match
  columns come back when the match starts.
- Ties: shared places, the banner with several leaders on the last level; a long server name
  next to the banner, and the banner laid out again when the leader changes; the `ФИНАЛ` badge
  after the weapon on the last level only.
- A spectator who is on the last level doesn't bring the banner while the leader isn't.
- Teamplay with 2 and 4 teams: team headers with level, weapon, progress and the badge after the
  weapon, player rows with kills only, players by kills, spectators at the bottom, the banner names
  the team. At 640×480 the kills column and Ping both fit.
- `&` in the title, a column title, a `Text` cell, a team cell, a badge and a banner row: each
  shows `&` and is not cut.
- After the win both banner rows are gone.
- Weapon icons in the scoreboard and the level strip with the OpenGL and the software renderer.
- A weapon with a non-standard sprite shows the name from the config.

GunGame HUD:

- The new elements match screenshots of the draft 1 widget at 640×480, 800×600, 1280×720,
  1920×1080 and 2560×1440, with 9, 14 and 24 levels and 3 and 25 kills per level.
- Cells shrink, then the window with `‹ ›` appears; the strip is lifted when it doesn't fit
  between armor and ammo, with `hud_custom` 0 and 1.
- Warmup → countdown → game → win: the block changes, the plugin texts are not duplicated, the
  block is empty after the win.
- Ties, a single player on the server, teamplay with 2 and 4 teams.
- Spectator in first-person, chase, map chase, roaming and free map modes; a dead player.
- A player who changes `lang` gets the texts in his new language.
- Kills per level ≤ 10 and > 10 (teamplay): dots and bar have the same length, the block doesn't jump.
- The timer, the next map line and custom timers are hidden while GunGame runs and come back
  after `Disable_GunGame` and with `hud_server_elements 0`. The speedometer and `hud_jumpspeed`
  stay in both positions.
- `gg_box` is behind the place, rival and kills in the match and behind the place alone in the
  warmup, and fades with them.
- `gg_place` pulses red in the last 3 seconds of the warmup and of the countdown, never in the
  match or after the win; it flashes on a place change in the match only.
- `gg_rival` colors for overtake, tied and lead; `gg_levels` markers with several players on one
  level.
- `gg_top` with 2, 5 and 32 players, the own row after `…`, teams as rows in teamplay; hidden in the
  warmup.
- Events: own level-up, skip, last level and level down in the announcer; the last level, a level
  down and a new leader of others in the feed; level-ups of others below the last level give no
  line and stay in chat; no leader event in teamplay, with one participant or on a tie; a lead
  taken by kills or a descore alone; the first leader of a game after `gg_enable 0` and `1`; one
  line for a team level down by `gg_set_player_level`; several announcer lines of one frame follow
  each other; events also with the chat line turned off.
- A player with `hud_server_elements 0` gets every chat line and no events; turning it on stops
  the chat lines that the feeds show.
- Feed and announcer lines in English, Russian and Spanish.
