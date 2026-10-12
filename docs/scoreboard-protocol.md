# Scoreboard and HUD extension protocol

Status: Draft 2, not released. Protocol version 1. Covers the BugfixedHL client and server,
`bugfixedapi_amxx` and the GunGame plugin.

This document describes how a BugfixedHL server sends to BugfixedHL clients a custom scoreboard
layout, extra values for players, teams and the whole game, and HUD elements defined by server
plugins. The client knows no game mode: a mode such as GunGame builds its scoreboard and HUD from
generic parts through plugin natives. The first consumer is the GunGame plugin (9.3).

## 1. Goals

- A server plugin can add columns to the scoreboard, hide some standard columns, move the others
  and fill cells for players, teams and the whole game.
- A server plugin can put HUD elements anywhere on the screen: text lines, icons, meters and icon
  strips. It chooses the position, alignment, color and font of each element, when it is shown,
  and its texts. Texts show cell values and places computed by the client.
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
the player it looks at.

| Term     | Meaning                                                                   |
|----------|---------------------------------------------------------------------------|
| Schema   | `Config` plus the list of columns                                         |
| Target   | Owner of a value: the whole game, a player or a team                      |
| Cell     | One value, addressed by target and column                                 |
| List     | A named array of values that belongs to the whole game                    |
| Element  | A HUD item defined by the server: `Text`, `Icon`, `Meter` or `Strip`      |
| Template | Text of an element with placeholders, evaluated by the client (8.10)      |
| Variant  | One of up to 8 templates of an element, chosen by a value                 |
| Viewer   | The client that draws the HUD                                             |
| Subject  | The player whose data an element shows: the viewer or the observed player |
| Caps     | Bitmask of client features, reported by the client in `_bhl`              |

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
release, bit 1 and `Config` changed meaning; draft 1 builds are not compatible (11).

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
  trailing bytes after the known fields are ignored. Later versions may append fields to them.
  `Values` and `List` have no room for trailing fields and are extended through new value types or
  new opcodes.

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
- `Icon.sprite` is a sprite name from `hud.txt`, the same names the kill feed uses (`d_9mmAR`).
  If the client doesn't have the sprite, it draws only `text`. For standard HL weapon sprites the
  client shows its own weapon name instead of `text` (10.3).
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
Element := op:byte(0x05) id:byte kind:byte
           flags:word x:short y:short align:byte place:byte parent:byte gap:byte
           color:byte{4} font:byte select:string value:string list:string
           count:byte Param{count}
Param   := id:byte value:short
```

| Field    | Type           | Notes                                                                                                  |
|----------|----------------|--------------------------------------------------------------------------------------------------------|
| `id`     | `byte`         | 0–31, assigned by the server. `0xFF` with kind `None` removes every element                            |
| `kind`   | `byte`         | 0 `None` (remove), 1 `Text`, 2 `Meter`, 3 `Strip`, 4 `Icon`; other values remove the element           |
| `flags`  | `word`         | See below                                                                                              |
| `x`      | `short`        | Horizontal position in 1/8192 of the screen width; `-8192` (−1.0) is the center (8.4)                  |
| `y`      | `short`        | Vertical position, the same way                                                                        |
| `align`  | `byte`         | Anchors: bits 0–1 horizontal, bits 2–3 vertical, see below                                             |
| `place`  | `byte`         | What the position is relative to, see below                                                            |
| `parent` | `byte`         | Element id for the parent places; `0xFF` otherwise or if the parent doesn't exist                      |
| `gap`    | `byte`         | Proportional units: distance to the parent, or the margin on the numbers row (8.4)                     |
| `color`  | `byte` × 4     | r, g, b, a. With `HudColor` only `a` is used                                                           |
| `font`   | `byte`         | Font id (8.5); unknown ids mean 0                                                                      |
| `select` | `string`       | `Text`: a ref whose number picks the variant (8.10); `""` means variant 0                              |
| `value`  | `string`       | The bound ref: `{@value}`, the progress of a `Meter`, the icon of an `Icon`, the position of a `Strip` |
| `list`   | `string`       | `Strip`: key of the list with the icons; `""` otherwise                                                |
| `count`  | `byte`         | Number of params, 0–16                                                                                 |
| `Param`  | `byte` `short` | Kind param id and value (8.7–8.9); unknown ids and params of other kinds are ignored, values clamped   |

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
  that ends before `font`. A missing string, or one without a NUL, means `""`. Params are read
  while bytes remain, at most `count` and at most 16; missing params take their defaults.
- A definition for an existing id replaces it. An `Element` never changes templates: the client
  drops the templates of an id only on a removal (kind `None` or an unknown kind, for that id or
  `0xFF`) and on `Reset`. When the server drops the texts of an element, it sends a removal right
  before the new definition (5.2).
- The largest message is a `Strip`, the kind with the most params: 18 fixed bytes, strings of 22,
  22 and 16 bytes and 1 + 11 × 3 bytes of params, 112 bytes.

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

The message takes 37 bytes. Variant 0 with a client token takes 18:

```
06 00 00                                       op Template, element 0, variant 0
23 42 48 4c 5f 48 55 44 5f 50 6c 61 63 65 00   "#BHL_HUD_Place"
```

### 4.6 Sizes for GunGame

| Message                                 | Bytes                         |
|-----------------------------------------|-------------------------------|
| `Reset`                                 | 2                             |
| `Config`                                | ~8                            |
| `Column` × 9                            | ~14–34                        |
| `List` `levels`, 24 levels              | 2 × ≤ 192                     |
| `Values`, 32 players                    | 7 × ≤ 192                     |
| `Element` × 6                           | 28–37, 192 in all             |
| `Template` × 9, plugin texts in Russian | 18–115, ~500 in all           |
| `HudState`                              | 7                             |
| Level-up of one player                  | ~33, +17 when `final` changes |
| Kill without level-up                   | ~18                           |
| Warmup second                           | ~8                            |
| Warmup start or end                     | ~140                          |

At the start and the end of the warmup the GunGame plugin sends the `level`, `weapon`, `progress`,
`kills` and `score` columns again and changes the visible elements (9.3). The banner needs no
traffic until the win: its rows are chosen by the leader's `level` cell (9.3).

### 4.7 Limits

| What                         | Limit                   |
|------------------------------|-------------------------|
| Message payload              | 192 bytes               |
| Columns                      | 32 (ids 0–31)           |
| Lists                        | 8 (ids 0–7)             |
| List items                   | 64                      |
| Column and list `key`        | 15 bytes                |
| Column title                 | 31 bytes                |
| Config title                 | 63 bytes                |
| Config tag                   | 15 bytes                |
| `Text` value                 | 31 bytes                |
| `Icon` sprite and text       | 31 bytes each           |
| Dots in a scoreboard `Meter` | `max ≤ 10`              |
| Elements                     | 32 (ids 0–31)           |
| Element key (server only)    | 15 bytes of `[a-z0-9_]` |
| Ref in `select` and `value`  | 21 bytes (4.3)          |
| Params per element           | 16                      |
| Variants per element         | 8                       |
| Template                     | 188 bytes               |
| Placeholders per template    | 16                      |
| Evaluated text               | 255 UTF-16 units        |

String limits are in bytes, without the terminating NUL.

## 5. Server behaviour

### 5.1 State

One object in `hl.so`, `CScoreboardExt`, holds the config, the columns, the lists, the cells of
the whole game, every player and every team, the HUD elements with their default templates and
visibility, the hidden standard HUD parts, and the per-player overrides of templates and
visibility (5.4). For each client it also keeps `caps`, their source, a `ready` flag (set after
`InitHUD`), pending `Reset`, header, elements, templates and HUD state, and sets of dirty cells
and list items.

- Column masks have 32 bits, one per column id. The mask of all columns is built without
  `1u << 32`, which is undefined in C++.
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
7. `HudState`, if it is pending, with the visibility for this client.

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
template this client should have and `HudState`, marked pending for that client. It starts:

- about 0.5 s after `InitHUD` of a client with `ScoreboardV1` (while the extension is inactive
  only `Reset` is sent);
- when `ScoreboardV1` appears after `InitHUD`;
- for every ready client, when a plugin turns the extension on in the middle of a map (a client
  that is still in its 0.5 s wait after `InitHUD` keeps it).

`InitHUD` already sends `ScoreInfo` and `TeamInfo` for every player and the MOTD. The engine
moves a client's reliable buffer (about 4 KB on HLDS) out only after the previous reliable message
is acknowledged, so the sync is paced by time, not by frames, to avoid `Reliable channel
overflowed`: it starts after the `InitHUD` burst and drains at about 5 KB/s. With 32 players a
GunGame sync is about 2.8 KB and takes about 0.6 s.

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

| Event                       | Server                                                                       |
|-----------------------------|------------------------------------------------------------------------------|
| Map change                  | State is cleared; the plugin sets everything again                           |
| Plugin resets the extension | `Reset` to every ready client; the extension is off                          |
| Plugin removes an element   | `Element` with kind `None`; `0xFF` when it removes all                       |
| Player disconnects          | Slot cells and overrides are cleared; `0xFF`/`None` for the slot to everyone |
| Player changes team         | Nothing: placement comes from `TeamInfo` and `ScoreInfo`                     |
| Client loses `ScoreboardV1` | Nothing more is sent to that client                                          |

A client dropped before it spawned (while it loads the map or downloads) counts as a disconnect
too; the server notices it in the next frame.

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

- `Icon`: the sprite from `hud.txt`, scaled to the row height with its aspect ratio kept and tinted
  with the row text color. The text follows to the right. The client reads the sprite file and
  draws it as a VGUI2 texture, so icons work with every renderer, the software one included.
  VGUI2 stops alpha blending once a row has drawn text, so the icon keeps the colors of the sprite
  and only its pixels brighter than 48 are drawn (an alpha test). HUD elements draw icons before
  any text and use blending there: the brightness of a pixel becomes its alpha, so the icon looks
  like an additive sprite. The software renderer always gets the alpha-tested icon.
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
- `hud_scoreboard_dump` prints the store: schema, cells, lists, elements, templates with their
  parse errors, the HUD state, the subject and the built-in values.
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

- a kind: `Text`, `Icon`, `Meter` or `Strip`;
- a place and a position (8.4), a color and a font (8.5);
- flags (4.3);
- refs to the data it shows: `select`, `value` and `list`;
- kind parameters, with defaults in `HudServerPanel` of `HudLayoutBase.res` that users can
  override in `HudLayout.res` (a param sent by the server wins);
- up to 8 template variants;
- a visibility bit set by the plugin.

Elements are drawn in two passes, each in id order: first the shapes and sprites of all elements
(`Meter` dots and bars, `Strip` cells and item icons, `Icon` sprites), then all texts (`Text`
elements, `Meter` counters, `Icon` texts, `Strip` item texts and scroll markers). A text is
therefore always drawn above any shape or sprite, whatever the ids.

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
   a `Strip` has `count > 0` and a numeric position.

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
- An element is unplaced when its parent is undefined, has a banner place, is the element itself,
  is part of a cycle or is unplaced. An unplaced element is not drawn and has no box.

Sizes and gaps are proportional units of a 640×480 screen, `P()`, like `.res` files. They don't
follow `hud_scale`. Positions are screen fractions, so `y = 0.025` (205) gives the same pixel as
`P(12)` at the common screen heights.

### 8.5 Color and fonts

- Color: with `HudColor` the rgb comes from `hud_color` (`HudPart::Common`), otherwise from the
  message.
- Alpha: `part · a / 255 · GetHudTransparency()`, where `part` is 255 or the alpha of a `Meter`
  or `Strip` part.
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
- No params.

### 8.7 `Meter`

`value` must be a `Progress` with `max > 0`; `cur` is clamped to `[0, max]`.

| Id | Param         | Default | Range | Meaning                                                             |
|----|---------------|---------|-------|---------------------------------------------------------------------|
| 1  | `dots`        | 10      | 0–32  | `max ≤ dots`: dots; otherwise a bar. 0: always a bar                |
| 2  | `dot_size`    | 7       | 2–64  | Diameter of a dot, height of the bar                                |
| 3  | `dot_gap`     | 3       | 0–64  | Distance between dots                                               |
| 4  | `bar_wide`    | 0       | 0–640 | Length of the bar; 0 is the length of `dots` dots, or 10 for 0 dots |
| 5  | `counter`     | 1       | 0–2   | Counter: 0 never, 1 with the bar, 2 always                          |
| 6  | `counter_gap` | 6       | 0–64  | Distance between the box and the counter                            |
| 7  | `alpha_empty` | 80      | 0–255 | Empty part of the bar                                               |

- After scaling a dot is `dot = max(2, P(dot_size))`.
- Dots: `max` dots of `dot` with `P(dot_gap)` between them. Dots before `cur` are filled
  anti-aliased discs, the rest are rings with a width of 1/6 of the diameter, all at alpha 255.
- Bar: a rounded bar (half-disc caps) at `alpha_empty`. When `cur > 0`, a fill of width
  `max(w · cur / max, dot)` at alpha 255.
- The box is the row of dots or the bar. Without a `Progress` value with `max > 0` the box is
  0 × `dot` and nothing is drawn. The counter stands outside the box, so the anchor keeps the box
  in place when the form changes.
- Counter:
  - the text is variant 0, or `{@value:cur} / {@value:max}` when the element has no variant 0;
    variants 1–7 are never used;
  - it uses the element font and is centered vertically on the box;
  - its left edge is at `box right + P(counter_gap)`; when the resolved horizontal anchor of the
    box is the right edge (an explicit right anchor, an auto anchor with a right-edge reference
    point (8.4) or the place `ParentLeft`), its right edge is at `box left − P(counter_gap)`.

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

Inputs:

- `count` is the number of items of `list`, or the `max` of the `value` `Progress` without a list
  (also when no list has that key); at most 64.
- The position is the `value`: `Progress.cur` or another number (8.10), 1-based.
  `cur = clamp(position − 1, 0, count − 1)`.
- Items:
  - an `Icon` item draws its sprite: `gHUD.GetSpriteIndex(sprite)`, cached until the store
    changes; a sprite the client doesn't have leaves the cell empty;
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

### 8.9 `Icon`

| Id | Param       | Default | Range | Meaning                                                 |
|----|-------------|---------|-------|---------------------------------------------------------|
| 30 | `icon_tall` | 0       | 0–255 | Height of the icon; 0 is the height of the element font |
| 31 | `icon_gap`  | 2       | 0–64  | Distance between the icon and the text                  |

- `value` must be an `Icon` cell. Its sprite from `hud.txt` is scaled to `P(icon_tall)` or the font
  height with its aspect ratio kept, tinted with the element color (8.5) and drawn like the icons
  of the scoreboard (6.6).
- Variant 0, if the element has it, is drawn after the icon at `P(icon_gap)`, in the element font,
  centered vertically on the icon: `{@value}` writes the name of the icon (8.10). Without variant 0
  only the icon is drawn.
- If the client doesn't have the sprite, only the text is drawn.
- The box holds the icon, the gap and the text in a row and is as tall as the higher of the icon
  and the font. The gap counts only when both the icon and the text are drawn. Without an `Icon`
  value the element has no content, not even its text, and the box is 0 × the font height.

### 8.10 Templates

A template is a text with placeholders. The client evaluates it every frame for the element's
subject.

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
  bit, so a plugin can tell which clients understand them.
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
  banner alike.
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
- `Strip`: templates are not used.

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

## 9. Plugin API

### 9.1 Server interface

`IBugfixedServer` gets minor version 3. Draft 1 builds report 1.2 with an older `IScoreboardExt`,
and calling it with the new layout would crash the server, so the module requires 1.3. New methods
are appended:

- `GetClientCaps(int idx)` returns the caps of a client;
- `GetScoreboard()` returns `bhl::IScoreboardExt *`, a separate interface whose methods take
  column, list and element keys, never ids. New methods are appended to it the same way.

`IScoreboardExt` is not released, so `SetConfig` changes in place, and `SetClock`, `AddBadge` and
the HUD methods follow the list methods:

```cpp
virtual void SetConfig(unsigned int hideFlags, const char *title, const char *tag) = 0;

// ... columns, cells and lists as before ...

virtual void SetClock(int target, const char *key, int ms, unsigned int flags) = 0;

//! host may name a column that is added later
virtual bool AddBadge(const char *key, const char *host) = 0;

//! Adds an element, or changes kind, position, font, anchors and flags of an existing one.
//! x and y follow set_hudmessage. A kind change drops templates, refs and params.
//! Bit 0 of flags is ignored: the color mode comes from HudSetColor.
virtual bool HudAdd(const char *key, int kind, float x, float y, int font, int align, unsigned int flags) = 0;

//! parent may name an element that is added later
virtual bool HudSetPlace(const char *key, int place, const char *parent, int gap) = 0;

//! r < 0: follow hud_color
virtual bool HudSetColor(const char *key, int r, int g, int b, int a) = 0;
virtual bool HudBind(const char *key, const char *value, const char *list, const char *select) = 0;
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
```

`bugfixedapi_amxx` checks the interface version per feature instead of refusing the whole API.
On a server older than 1.3 the existing natives keep working, `bhl_sb_available()` returns false,
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
	BHL_HUD_ICON = 4
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
	BHL_HUD_P_ICON_GAP = 31
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
- `bhl_sb_reset()` removes everything: schema, cells, lists, elements, texts, visibility and hidden
  standard parts.
- `bhl_get_client_caps()` returns the normalized caps (3.1): `BHL_CAPS_HUD_V1` only together with
  `BHL_CAPS_SCOREBOARD_V1`, and 0 for bots and HLTV.

HUD:

- Element keys are up to 15 characters of `[a-z0-9_]`, in their own namespace. `bhl_hud_add()`
  returns false for an invalid key or kind, or when 32 elements exist. The other `bhl_hud_*`
  natives return false for an unknown key. A `bhl_hud_*` native that returns false logs one line
  for that call. `bhl_hud_remove()` ignores an unknown key and logs nothing.
- A new element is shown, is placed on the screen, follows `hud_color` with alpha 255 and has no
  refs, params or texts. `bhl_hud_add()` on an existing key changes its kind, position, font,
  anchors and flags and keeps the rest; a kind change drops its texts, the per-player ones
  included, its refs and params, and keeps its place, color and visibility. The color mode comes
  only from `bhl_hud_color()`.
- `x` and `y` work like in `set_hudmessage`; hl.so sends them in 1/8192, rounded (4.3).
- `bhl_hud_place()` takes the key of the parent, which may be added later. While no element has
  that key, the element is not drawn (8.4). The parent is ignored for places that are not parent
  places.
- `bhl_hud_color()` with `r = BHL_HUD_COLOR` follows `hud_color` and uses only `a`.
- `bhl_hud_bind()` sets the refs: `value` and `select` are a column key or a built-in value, with an
  optional `^` and one of `cur`, `max` and `left` (`@state`, `level:left`); `list` is a list key.
- `bhl_hud_param()` returns false for a param of another kind and clamps the value to the range of
  the param (8.7–8.9).
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

| Key          | Kind                 | Target         | Type       | Meaning                                         |
|--------------|----------------------|----------------|------------|-------------------------------------------------|
| `rank`       | builtin column       | —              | —          | Place, computed by the client                   |
| `level`      | `Steps`, `MarkLast`  | player or team | `Progress` | Current level, 1-based / number of levels       |
| `weapon`     | `Icon`               | player or team | `Icon`     | Weapon of the current level                     |
| `progress`   | `Meter`              | player or team | `Progress` | Kills on the current level / kills it needs     |
| `final`      | badge after `weapon` | player or team | `Text`     | `#BHL_SB_Final` on the last level               |
| `kills`      | `Text`               | player, team   | `Int`      | Kills in the warmup, then in the match          |
| `score`      | `Text`, hidden       | player, team   | `Int`      | Progress P, first sort key in the match         |
| `phase`      | `Text`, hidden       | the whole game | `Int`      | 0 game, 1 warmup, 2 start countdown, 3 finished |
| `phase_left` | `Text`, hidden       | the whole game | `Int`      | Seconds left in the warmup or the countdown     |
| `levels`     | list                 | the whole game | `Icon`     | Weapon of every level, in order                 |

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

| Key             | Kind               | Place                   | Refs                               | Variants                      | Shown         |
|-----------------|--------------------|-------------------------|------------------------------------|-------------------------------|---------------|
| `gg_place`      | `Text`, large font | top center, `y 0.025`   | `select phase`, `value phase_left` | place, warmup, countdown      | always        |
| `gg_rival`      | `Text`             | under `gg_place`, gap 2 | `select @state`                    | waiting, overtake, tied, lead | match         |
| `gg_kills`      | `Meter`            | under `gg_rival`, gap 6 | `value progress`                   | —                             | match         |
| `gg_levels`     | `Strip`            | numbers row, gap 3      | `value level`, `list levels`       | —                             | match         |
| `gg_banner_top` | `Text`, `Leader`   | banner top              | `select level:left`                | title                         | until the win |
| `gg_banner_bot` | `Text`, `Leader`   | banner bottom           | `select level:left`                | leader line                   | until the win |

- "Match" means the plugin shows the element while `phase` is 0, "until the win" while `phase` is
  not 3.
- `phase` 3 has no variant in `gg_place`, so after the win the place block is empty.
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
	
	bhl_hud_add("gg_rival",BHL_HUD_TEXT)
	bhl_hud_place("gg_rival",BHL_HUD_PARENT,"gg_place",2)
	bhl_hud_bind("gg_rival",.select = "@state")
	
	bhl_hud_add("gg_kills",BHL_HUD_METER)
	bhl_hud_place("gg_kills",BHL_HUD_PARENT,"gg_rival",6)
	bhl_hud_bind("gg_kills","progress")
	
	bhl_hud_add("gg_levels",BHL_HUD_STRIP)
	bhl_hud_place("gg_levels",BHL_HUD_NUMBERS_ROW,"",3)
	bhl_hud_bind("gg_levels","level","levels")
	
	bhl_hud_add("gg_banner_top",BHL_HUD_TEXT,.flags = BHL_HUD_LEADER)
	bhl_hud_place("gg_banner_top",BHL_HUD_BANNER_TOP)
	bhl_hud_bind("gg_banner_top",.select = "level:left")
	
	bhl_hud_add("gg_banner_bot",BHL_HUD_TEXT,.flags = BHL_HUD_LEADER)
	bhl_hud_place("gg_banner_bot",BHL_HUD_BANNER_BOTTOM)
	bhl_hud_bind("gg_banner_bot",.select = "level:left")
	
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
	bhl_sb_set_int(BHL_SB_GLOBAL,"phase_left",timeLeft)
	
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
	
	// After the win the leader line would read "0 kills to win"
	bhl_hud_show(0,"gg_banner_top",!won)
	bhl_hud_show(0,"gg_banner_bot",!won)
}

bool:Player_HasBhlHud(id){
	return bhlApi && (bhl_get_client_caps(id) & BHL_CAPS_HUD_V1) != BHL_CAPS_NONE
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

`gungame.txt` gets the texts. Other languages follow the same pattern; a language with two forms
uses two. The texts go through `%L`, so a literal `%` in them is written `%%`.

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
```

The longest text, the Russian banner line, takes 111 bytes. A plugin that doesn't want to send
texts to every player can use the client tokens instead (10.1), in English and Russian only:
`bhl_hud_text(0,"gg_place",0,"#BHL_HUD_Place")`.

| Plugin function          | Calls                                                                            |
|--------------------------|----------------------------------------------------------------------------------|
| `Enable_GunGame`         | `Scoreboard_Setup()`                                                             |
| `Disable_GunGame`        | `bhl_sb_reset()`; the standard HUD parts come back                               |
| `client_putinserver`     | `Scoreboard_UpdatePlayer(id)`; `Hud_SetTexts(id)` except for bots and HLTV       |
| `client_infochanged`     | `Hud_SetTexts(id)` when `lang` changed, only for a connected player              |
| `Equip_PlayerWithWeapon` | `Scoreboard_UpdatePlayer(id)`                                                    |
| Descore (suicide)        | `Scoreboard_UpdatePlayer(victim)`                                                |
| Kill and level-up        | `Scoreboard_UpdatePlayer(killer)`, team cells in teamplay                        |
| `Update_TeamData`        | Team cells through `bhl_sb_team(teamName)`                                       |
| `WarmUp_Timer`           | `Scoreboard_SetPhase(left)` every second                                         |
| Match start              | `Scoreboard_SetPhase(0)`                                                         |
| `resetPlayersScore`      | `kills` 0 for everyone                                                           |
| Disconnect               | Team `kills` without the player                                                  |
| `endgame`                | Final player and team cells; `Scoreboard_SetPhase(0)` sets phase 3 (`isEndGame`) |
| Auto-save restore        | `Scoreboard_UpdatePlayer(id)`                                                    |

The engine calls `client_infochanged` before `ClientConnect` too, when no text can be set for the
slot (9.2). So the plugin keeps the `lang` a player's texts were set for: it stores it in
`client_putinserver` after `Hud_SetTexts(id)`, compares it in `client_infochanged` and clears it on
disconnect.

For players with `HudV1` (`Player_HasBhlHud`) the plugin skips:

- the text informer (`Show_HudInformer`);
- `StatusIcon`;
- the warmup messages from `WarmUp_Timer`, which then go to every other player one by one
  instead of `show_hudmessage(0, …)`.

`bhl_client_caps_changed` switches these on and off when a player changes `hud_server_elements`.
The winner message, the start message (`WARMUP_ROUND_OVER`), the final table and chat
notifications stay for everyone.

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

| Client                         | Scoreboard                     | HUD                                                      |
|--------------------------------|--------------------------------|----------------------------------------------------------|
| Old                            | standard                       | Plugin informer, `StatusIcon`, warmup texts              |
| New, defaults                  | GunGame columns, banner, badge | Elements; plugin texts off except winner, start and chat |
| New, `hud_server_elements 0`   | GunGame columns, banner, badge | Plugin informer, `StatusIcon`, warmup texts; timer shown |
| New, `hud_scoreboard_custom 0` | standard                       | As with the defaults                                     |
| New, both 0                    | standard                       | Plugin informer, `StatusIcon`, warmup texts; timer shown |

On a client that draws the elements, GunGame hides the BugfixedHL timer, the next map line and
custom timers (8.12). The bottom speedometer and `hud_jumpspeed` stay and can cross the level
strip.

Module and server DLL:

| `bugfixedapi_amxx` \ `hl.so` | API 1.0–1.2                                  | API 1.3    |
|------------------------------|----------------------------------------------|------------|
| Old module                   | as now                                       | as now     |
| New module                   | old natives work, no `bhl_sb_*`, `bhl_hud_*` | everything |
| No module                    | GunGame works as now                         | same       |

Draft 1 test builds are not supported: the client, `hl.so`, the module and the plugin are updated
together. Their `_bhl` bit 1 and `Config` mean something else, and their `hl.so` reports API 1.2,
which the new module refuses (9.1).

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
