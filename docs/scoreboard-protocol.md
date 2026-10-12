# Scoreboard extension protocol

Protocol version 1. Implemented in the BugfixedHL client and server, in `bugfixedapi_amxx` and in the
GunGame plugin.

This document describes how a BugfixedHL server sends a custom scoreboard layout and extra values
for players, teams and the whole game to BugfixedHL clients, and how the GunGame scoreboard and
HUD widget use that data. The first consumer is the GunGame plugin.

## 1. Goals

- A server plugin can add columns to the scoreboard, hide some standard columns, move the others
  and fill cells for players, teams and the whole game.
- HUD widgets read the same data. The first one is the GunGame HUD (8.4).
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
plugin (AMXX) ── natives ──▶ bugfixedapi_amxx ── IBugfixedServer ──▶ hl.so: scoreboard state
                                                                          │
                                              "BhlScore" user message, MSG_ONE,
                                              only to clients with ScoreboardV1
                                                                          ▼
                                     client.dll: scoreboard store ──▶ ScorePanel, hud_scores,
                                                                      GunGame HUD widget
```

| Term   | Meaning                                                        |
|--------|----------------------------------------------------------------|
| Schema | `Config` plus the list of columns                              |
| Target | Owner of a value: the whole game, a player or a team           |
| Cell   | One value, addressed by target and column                      |
| List   | A named array of values that belongs to the whole game         |
| Preset | Client-side look and behaviour bound to a name, e.g. `gungame` |
| Caps   | Bitmask of client features, reported by the client in `_bhl`   |

## 3. Client capabilities

### 3.1 Format

The client reports its features in the cvar and userinfo key `_bhl`. The value is a bitmask
written as lowercase hex without a prefix, at most 8 digits: `1`, `3`, `1f`.

The server parses it with `strtoul(value, &end, 16)`. An empty value, trailing characters or more
than 8 digits mean `0`. Unknown bits are ignored.

| Bit  | Mask  | Name           | Meaning                                                  |
|------|-------|----------------|----------------------------------------------------------|
| 0    | `0x1` | `ScoreboardV1` | Understands `BhlScore` protocol version 1                |
| 1    | `0x2` | `GunGameHudV1` | Draws the GunGame HUD widget; the plugin drops its texts |
| 2–31 | —     | reserved       | Clients send 0, servers ignore                           |

`GunGameHudV1` is only set together with `ScoreboardV1`, because the widget reads the store.

Bits are never reused or redefined. An incompatible protocol change gets a new bit
(`ScoreboardV2`), and the server sends the new format only to clients that have that bit.

`_bhl` doesn't replace `aghl_supports`. The MOTD flags stay where they are.

### 3.2 Client side

- `ConVar _bhl` has only `FCVAR_USERINFO`.
  - No `FCVAR_ARCHIVE` or `FCVAR_BHL_ARCHIVE`: the value is computed, it must not be saved to configs.
  - No `FCVAR_PRIVILEGED`: the engine answers a cvar query for such cvars with `CVAR is privileged`,
    which would break the server fallback from 3.3.
- The cvar is registered with the caps of the build as its default value, so the key is in
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
  - `hud_scoreboard_custom 0` clears `ScoreboardV1` and `GunGameHudV1`. The server stops sending
    data, the client shows the standard scoreboard. Setting it back to 1 brings a full sync.
  - `hud_gungame 0` clears `GunGameHudV1`. The plugin shows its own HUD texts again.

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
   to `Unknown`.
2. In `InitHUD`, if the source is `Unknown` and either a schema is active or `sv_bhl_query_vars`
   is 1, the server calls `QueryClientCvarValue2(_bhl)` with a new `REQUESTID_CAPS`. It applies the
   same p47 and LAN checks as the existing `aghl_*` queries. If a schema becomes active later on the
   map, clients that are still `Unknown` are queried at that moment.
3. An answer that doesn't parse (`Bad CVAR request`, `CVAR is privileged`, empty) means `0`.
4. Every change of `caps` fires the plugin forward `bhl_client_caps_changed(id)`.
   - `ScoreboardV1` appeared after `InitHUD`: the client gets a full sync (5.3).
   - `ScoreboardV1` disappeared: the server stops sending to that client and doesn't send `Reset`.

Each layer covers a different case:

| Layer       | Covers                                                              |
|-------------|---------------------------------------------------------------------|
| Cvar mirror | The normal case: no extra traffic, known before `InitHUD`           |
| `setinfo`   | The cvar was not mirrored into userinfo for any reason              |
| Cvar query  | Userinfo is full (256 bytes), so neither path above can add the key |

The server command `bhl_caps` prints slot, name, caps and source for every connected client.

## 4. Message `BhlScore`

### 4.1 Transport

- The server registers `REG_USER_MSG("BhlScore", -1)`: variable size, payload up to 192 bytes.
- The message is sent only with `MSG_ONE` (reliable) and only to clients that have
  `ScoreboardV1` and passed `InitHUD`. It is never sent with `MSG_ALL`, `MSG_BROADCAST` or
  `MSG_ONE_UNRELIABLE`, and never to bots or HLTV proxies.
  - Old clients never receive it, so it doesn't matter how their engine handles unknown messages.
  - An old server never registers it, so the client hook never fires.
- One message carries one operation. The first byte is the opcode, the rest is its payload.
  The client ignores messages with unknown opcodes, which lets new opcodes be added safely.
- `Reset`, `Config` and `Column` are read field by field, and trailing bytes after the known fields
  are ignored. Later versions may append fields to them. `Values` and `List` have no room for
  trailing fields and are extended through new value types.

### 4.2 Data types

All integers are little-endian, as written by `WRITE_*` and read by `READ_*`.

| Type     | Bytes | Write / read                   | Range                 |
|----------|-------|--------------------------------|-----------------------|
| `byte`   | 1     | `WRITE_BYTE` / `READ_BYTE`     | 0 … 255               |
| `char`   | 1     | `WRITE_CHAR` / `READ_CHAR`     | −128 … 127            |
| `short`  | 2     | `WRITE_SHORT` / `READ_SHORT`   | −32768 … 32767        |
| `word`   | 2     | `WRITE_SHORT` / `READ_WORD`    | 0 … 65535             |
| `long`   | 4     | `WRITE_LONG` / `READ_LONG`     | −2³¹ … 2³¹−1          |
| `string` | n + 1 | `WRITE_STRING` / `READ_STRING` | UTF-8, NUL-terminated |

Strings:

- A string starting with `#` is a localization token resolved by VGUI2 on the client. If the token
  is missing, the client shows the string without the leading `#`.
- Plugins can only use tokens shipped with the client in `ui/resource/language/bugfixedhl_*.txt`.
  The client adds generic tokens for common columns, see section 10.
- Color codes (`^1` … `^9`) are not interpreted in version 1. The client strips them.
- The server truncates strings to the limits from 4.6. The client truncates them again
  if they are still too long.

### 4.3 Opcodes

| Op     | Name     | Payload                                     |
|--------|----------|---------------------------------------------|
| `0x00` | `Reset`  | Protocol version                            |
| `0x01` | `Config` | Hidden standard columns, preset, title, tag |
| `0x02` | `Column` | One column definition                       |
| `0x03` | `Values` | Cells for one or more targets               |
| `0x04` | `List`   | Items of one list                           |
| other  | reserved | Ignored by the client                       |

#### `Reset` (`0x00`)

| Field     | Type   | Notes                 |
|-----------|--------|-----------------------|
| `version` | `byte` | Protocol version, `1` |

The client drops the schema, all cells and all lists and returns to the standard scoreboard.
`version` is informational: the client reads the fields it knows. A version 1 client accepts any
`version ≥ 1` and ignores a `Reset` with `version 0`.

#### `Config` (`0x01`)

| Field    | Type     | Notes                                                |
|----------|----------|------------------------------------------------------|
| `flags`  | `word`   | See below                                            |
| `preset` | `string` | Up to 15 bytes; `""` means the generic look          |
| `title`  | `string` | Up to 63 bytes; replaces the server name if not `""` |
| `tag`    | `string` | Up to 15 bytes; a badge after the server name: `FFA` |

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
A repeated `Config` replaces the previous one.

`preset` selects client-side look and behaviour, see 6.7. An unknown preset falls back to the
generic look.

#### `Column` (`0x02`)

| Field     | Type     | Notes                                                                            |
|-----------|----------|----------------------------------------------------------------------------------|
| `id`      | `byte`   | 0–15                                                                             |
| `display` | `byte`   | How cells are drawn, see below                                                   |
| `flags`   | `word`   | See below                                                                        |
| `width`   | `byte`   | Proportional units as in `ScorePanel.res`; `0` is the default                    |
| `sort`    | `byte`   | `0`: not a sort key; bits 0–6: priority, 1 first; bit 7: ascending               |
| `key`     | `string` | Up to 15 bytes of `[a-z0-9_]`, unique: `level`, `weapon`                         |
| `title`   | `string` | Up to 31 bytes, text or `#token`; `""` for a built-in column keeps its own title |

A `Column` with an existing `id` replaces that column's definition; its cells are kept.
Columns are shown in ascending `id` order. The server assigns ids, plugins address columns
by `key`. Presets and HUD widgets use `key` to find data by meaning without depending on ids.

Display kinds:

| Value | Name      | Draws                                                                            |
|-------|-----------|----------------------------------------------------------------------------------|
| 0     | `Text`    | Any cell as text                                                                 |
| 1     | `Icon`    | `Icon` cells as sprite and text; other cells as text                             |
| 2     | `Meter`   | `Progress` cells as dots or a bar with a `cur/max` counter (6.6); others as text |
| 3     | `Builtin` | A standard column, chosen by `key`; cells are ignored                            |
| 4     | `Steps`   | `Progress` cells as `cur/max` with a strip of `max` steps below (6.6)            |
| other | —         | Same as `Text`                                                                   |

Default widths for `width = 0`: `Text` 60, `Icon` 100, `Meter` 90, `Steps` 60. A `Builtin` column
keeps the width of its standard column.

Built-in columns:

| `key`     | Column  | Notes                                                   |
|-----------|---------|---------------------------------------------------------|
| `rank`    | `#`     | Place, computed by the client (6.4); always drawn first |
| `frags`   | Frags   |                                                         |
| `deaths`  | Deaths  |                                                         |
| `eff`     | Eff     | Also follows `hud_scoreboard_showeff`                   |
| `steamid` | SteamID | Also follows `hud_scoreboard_showsteamid`               |
| `ping`    | Ping    |                                                         |
| other     | —       | The column is ignored                                   |

A `Builtin` column moves a standard column to its own position. Standard columns that the schema
doesn't mention keep their default places. `Hide*` flags win over a `Builtin` column.

Column flags:

| Bits | Name      | Meaning                                                     |
|------|-----------|-------------------------------------------------------------|
| 0    | `Hidden`  | Data only: not drawn, still used for sorting and by widgets |
| 1–2  | `Align`   | 0 left, 1 center, 2 right; 3 is treated as left             |
| 3    | `Primary` | The main score: `hud_scores` shows it instead of frags      |
| 4–15 | reserved  | Sent as 0, ignored                                          |

A `Hidden` column can carry values for the whole game only, for example the GunGame phase.

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
| other | —          | any     | Unknown type: the cell is cleared                      |

- A known type with a wrong `len` or a broken payload clears the cell.
- If the message ends in the middle of a group or a cell, the client keeps the cells it has
  already applied and drops the rest.
- Cells of columns that don't exist yet are stored and shown when the column arrives.
- `Icon.sprite` is a sprite name from `hud.txt`, the same names the kill feed uses (`d_9mmAR`).
  If the client doesn't have the sprite, it draws only `text`.
- `Progress` with `max = 0` is drawn as `cur`.

Sort keys: `Int` uses the value, `Progress` uses `cur`, `Text` and `Icon` compare text
case-insensitively. Empty cells go after filled ones.

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

### 4.5 Sizes for GunGame

| Message                    | Bytes     |
|----------------------------|-----------|
| `Reset`                    | 2         |
| `Config`                   | ~17       |
| `Column` × 8               | ~13–33    |
| `List` `levels`, 24 levels | 2 × ≤ 192 |
| `Values`, 32 players       | 7 × ≤ 192 |
| Level-up of one player     | ~33       |
| Kill without level-up      | ~18       |
| Warmup second              | ~8        |
| Warmup start or end        | ~130      |

At the start and the end of the warmup the GunGame plugin sends the `level`, `weapon`, `progress`,
`kills` and `score` columns again (8.1).

### 4.6 Limits

| What                   | Limit         |
|------------------------|---------------|
| Message payload        | 192 bytes     |
| Columns                | 16 (ids 0–15) |
| Lists                  | 8 (ids 0–7)   |
| List items             | 64            |
| Column and list `key`  | 15 bytes      |
| Column title           | 31 bytes      |
| Preset name            | 15 bytes      |
| Config title           | 63 bytes      |
| Config tag             | 15 bytes      |
| `Text` value           | 31 bytes      |
| `Icon` sprite and text | 31 bytes each |
| Dots                   | `max ≤ 10`    |

String limits are in bytes, without the terminating NUL.

## 5. Server behaviour

### 5.1 State

One object in `hl.so`, `CScoreboardExt`, holds the config, the columns, the lists and
the cells of the whole game, every player and every team. For each client it also keeps `caps`,
their source, a `ready` flag (set after `InitHUD`), pending `Reset` and header flags and sets of
dirty cells and list items.

### 5.2 Sending

Plugins only change the state and mark the changed cells and list items dirty for every ready
client. In `StartFrame` the server walks the ready clients that have `ScoreboardV1` and sends:

1. `Reset`, if one is pending;
2. `Config`, if it is pending, and every pending `Column`;
3. the dirty list items packed into `List` messages;
4. the dirty cells packed into `Values` messages.

A client gets at most about 512 bytes per frame. When its budget is used up, nothing more is sent
to it for about 0.1 s. A client with nothing pending gets small updates every frame. Several
changes of one cell in one frame produce one value.

### 5.3 Full sync

A full sync is `Reset`, the header, every list item and every non-empty cell marked dirty for
that client. It starts:

- about 0.5 s after `InitHUD` of a client with `ScoreboardV1` (without a schema only `Reset` is
  sent);
- when `ScoreboardV1` appears after `InitHUD`;
- for every ready client, when a plugin turns a schema on in the middle of a map (a client that
  is still in its 0.5 s wait after `InitHUD` keeps it).

`InitHUD` already sends `ScoreInfo` and `TeamInfo` for every player and the MOTD. The engine
moves a client's reliable buffer (about 4 KB on HLDS) out only after the previous reliable message
is acknowledged, so the sync is paced by time, not by frames, to avoid `Reliable channel
overflowed`: it starts after the `InitHUD` burst and drains at about 5 KB/s. With 32 players a
GunGame sync takes about 0.6 s.

### 5.4 Events

| Event                | Server                                                         |
|----------------------|----------------------------------------------------------------|
| Map change           | State is cleared; the plugin sets the schema again             |
| Plugin resets schema | `Reset` to every ready client; the schema is off               |
| Player disconnects   | Slot cells are cleared; `0xFF`/`None` for the slot to everyone |
| Player changes team  | Nothing: placement comes from `TeamInfo` and `ScoreInfo`       |
| Client loses the cap | Nothing more is sent to that client                            |

## 6. Client behaviour: scoreboard

### 6.1 Store

The store, `CScoreboardData`, is cleared on `VidInit` and on `Reset`. A player's cells
are cleared when the slot becomes empty, even if the server didn't say so.

### 6.2 Activation

The custom scoreboard is shown when a `Config` arrived after the last `Reset` or `VidInit` and
`hud_scoreboard_custom` allows it. Otherwise the standard scoreboard is shown; incoming data is
still stored.

### 6.3 Layout

- Column order: `rank`, Avatar, Name, then SteamID, Eff, Frags and Deaths at their default places
  unless the schema moves them, then custom and `Builtin` columns by `id`, then Ping unless moved.
  Hidden standard columns and `Hidden` custom columns are left out.
- The panel keeps the size, place, frame and header of the standard scoreboard (`ScorePanel.res`).
- Name takes all free width. If the other columns need more, Name shrinks down to 100 units
  (`column_name_min`), then down to 80 units; columns that still don't fit are dropped from the
  end.
- While a `Steps` column is visible, rows are taller so the strip fits under the text.
- Team section headers show team cells as text in the same columns (`ModifyColumn`; the list
  control can't draw images in headers): `Icon` as its name, `Progress` as `cur/max`. A `Meter`
  column with `max ≤ 10` stays empty, like dots without a counter. Team frags and deaths follow
  the `Hide*` flags.

### 6.4 Sorting and places

- Players inside a section and the teams are sorted by the sort keys in priority order. Ties fall
  back to the standard order (frags descending, deaths ascending), then to the index. Teams are
  sorted by team cells; a team without cells keeps the standard order.
- The place in the `rank` column depends only on the first sort key: equal values share a place
  (`1, 1, 3`). Further keys only order the rows. Without sort keys the place follows row order.
- With team sections the place is shown in the team header and counted among teams; player rows
  leave `rank` empty. Without teams the place is shown in player rows.

### 6.5 `hud_scores`

With an active schema, `hud_scores` uses the first `Primary` column (lowest `id`) that isn't
`Hidden`. It shows that column as text instead of frags and sorts the same way as the scoreboard.
Without such a column it doesn't change.

### 6.6 Drawing

- `Icon`: the sprite from `hud.txt`, scaled to the row height with its aspect ratio kept and tinted
  with the row text color. The text follows to the right. The client reads the sprite file and
  draws it as a VGUI2 texture, so icons work with every renderer, the software one included.
  The brightness of a pixel becomes its alpha, so the icon looks like an additive sprite.
- `Meter`, the same element as in the GunGame HUD (8.4.3):
  - `max ≤ 10`: `cur` filled dots and `max − cur` empty ones, without a counter;
  - `max > 10`: a rounded bar of the same length as 10 dots, filled to `cur / max`, with the
    `cur/max` counter to the right.
- `Steps`: `cur/max` as text; below it a strip of `max` segments. Segments before `cur` are
  filled, segment `cur` is highlighted, the rest are dimmed, the last one is marked.
- `Progress` in a `Text` column: `cur/max`.

### 6.7 Presets

| Preset    | Behaviour                                        |
|-----------|--------------------------------------------------|
| `""`      | Generic: only what the schema describes          |
| `gungame` | The GunGame scoreboard and HUD widget, section 8 |

### 6.8 Debug commands

- `hud_scoreboard_test [preset|off] [phase] [phase_left]` fills the store with fake data for the
  current players without a server. With `phase` 1 or 2 it shows the warmup columns, like the
  GunGame plugin.
- `hud_scoreboard_dump` prints the store to the console.

## 7. Client behaviour: standard scoreboard

Without an active schema the scoreboard is the same as now. The place column appears only when a
schema adds the built-in `rank` column. With a schema the scoreboard keeps the standard look: size,
place, header, fonts, column titles and row highlights.

## 8. GunGame preset

The HUD widget follows the "Main" mockup made for 1920×1080. The scoreboard keeps the look of the
standard BugfixedHL scoreboard and only adds the GunGame columns, the tag badge and the leader
banner. Interface strings are localized (section 10); the examples use Russian.

### 8.1 Data

The plugin fills these keys:

| Key          | Kind    | Target         | Type       | Meaning                                         |
|--------------|---------|----------------|------------|-------------------------------------------------|
| `rank`       | builtin | —              | —          | Place, computed by the client                   |
| `level`      | column  | player, team   | `Progress` | Current level, 1-based / number of levels       |
| `weapon`     | column  | player, team   | `Icon`     | Weapon of the current level                     |
| `progress`   | column  | player, team   | `Progress` | Kills on the current level / kills it needs     |
| `kills`      | column  | player, team   | `Int`      | Kills in the warmup, then in the match          |
| `score`      | column  | player, team   | `Int`      | Progress P, hidden, first sort key in the match |
| `phase`      | column  | the whole game | `Int`      | 0 game, 1 warmup, 2 start countdown, 3 finished |
| `phase_left` | column  | the whole game | `Int`      | Seconds left in the warmup or the countdown     |
| `levels`     | list    | the whole game | `Icon`     | Weapon of every level, in order                 |

- P is the sum of kills needed on all passed levels plus kills on the current level. The plugin
  computes it, because kills per level differ (1–7 in the shipped configs) and teamplay multiplies
  them by `gg_teamplay_multigoal`. Ordering by P gives the same order as the plugin's
  `Sort_RankPos` (level, then kills).
- `kills` counts kills of other players. Suicides and team kills don't count. The counter is reset
  together with the scores when the match starts.
- `Config` hides the standard Frags, Deaths, Eff and SteamID columns and leaves the title empty, so
  the header shows the server name. The tag is `FFA` or `TEAM`.
- Levels are the same for everybody in the warmup. While `phase` is 1 or 2 the plugin sends the
  columns again: `level`, `weapon` and `progress` become `Hidden`, `kills` becomes visible,
  `Primary` instead of `level` and the first sort key, `score` the second one. When the match
  starts it sends the match columns: `kills` is `Hidden` and the second sort key.
- A player is on the last level when `level.cur = level.max`.
- Weapon names come from the client table (10.3) for standard HL sprites. `Icon.text` from the
  server is used only for other sprites.

### 8.2 Places

The scoreboard and the HUD use the same rules.

- Participants are players that are not spectators. Bots count.
- Places follow P: equal P share a place (`1, 1, 3`). In the warmup they follow `kills`.
- In teamplay places are counted among teams that have participants. Team names come from
  `TeamNames`, team P from the team `score` cell.
- Overtake: the neighbour is the nearest participant with a strictly greater P;
  N = P(neighbour) − P(player) + 1, at least 1.
- Lead: D = P(player) − P(next participant).

### 8.3 Scoreboard (TAB)

#### 8.3.1 Placement

- Size, place, frame and fonts of the standard scoreboard (`ScorePanel.res`).
- One row per player. When rows don't fit, the existing compact spacing is used, then scrolling.

#### 8.3.2 Header

The header of the standard scoreboard: the server name, the map below it and the player count on
the right. The tag badge from `Config` (`FFA` or `TEAM`) follows the server name.

The banner `Лидер на последнем уровне` stands left of the player count, on the lines of the server
and map names:

- shown while the player or team on the first place is on the last level;
- an accent bar, the banner title and the line `Kr1sTaL · Crowbar · 1 убийство до победы`:
  the leader's name, the weapon and `progress.max − progress.cur` kills left;
- in teamplay the line names the team;
- if several leaders share the first place, the first row in sort order is shown;
- the server name is cut so that it and the tag end before the banner.

#### 8.3.3 Columns

In the match: `#` · Avatar · Name · `Уровень` · `Оружие` · `Прогресс` · Ping

In the warmup: `#` · Avatar · Name · `Убийства` · Ping

- Column titles, the name and the ping are drawn as in the standard scoreboard.
- The name column takes all free width; the others have fixed widths.
- Avatars follow `hud_scoreboard_showavatars`. Frags, Deaths, SteamID and Eff are hidden.

| Column     | Shows                                                                                     |
|------------|-------------------------------------------------------------------------------------------|
| `#`        | Place as a number                                                                         |
| `Уровень`  | `9/14` with the strip of all levels below (`Steps`); the last level is marked             |
| `Оружие`   | Weapon icon and name. On the last level the `ФИНАЛ` badge follows the name                |
| `Прогресс` | Dots for kills on this level, filled for done; above 10 kills a bar and `16/25` (`Meter`) |
| `Убийства` | `kills`, aligned right; only in the warmup                                                |

#### 8.3.4 Highlighted rows

- The own row and the last killer are highlighted as in the standard scoreboard.
- A player on the last level: `ФИНАЛ` follows the weapon.

#### 8.3.5 Teamplay

- Sections per team, as in the current BugfixedHL scoreboard; sections are ordered by team P, in
  the warmup by team `kills`.
- A team header shows the team place, level, weapon and, above 10 kills, the progress counter;
  in the warmup the place and kills.
- Players inside a team are sorted by `kills`.
- Spectators stay in their own section at the bottom.

### 8.4 HUD widget

Health, armor, ammo, the kill feed and the crosshair stay the classic HL elements. The widget adds
two elements: the place block at the top center and the level bar at the bottom center.

#### 8.4.1 Visibility

- The widget is on when the `gungame` preset is active and both `hud_scoreboard_custom` and
  `hud_gungame` are 1.
- It follows the usual HUD rules: hidden with `hud_draw 0` and during intermission. It is also
  hidden while the scoreboard is open, because a long table reaches the top of the screen.
- A spectator in first-person mode sees the data of the observed player. In other spectator
  modes the widget is hidden.
- With `phase = 3` (finished) the widget is hidden; the plugin's winner message and final table
  are shown as before.
- While the widget is on, `CHudTimer` draws nothing, including the next map line and server
  timers. This includes the finished phase. `customtimer` still rings when it expires.
- While the level bar is on (also while the open scoreboard hides it), the speedometer and
  `hud_jumpspeed` in their bottom mode and the status bar move above it.

#### 8.4.2 Place block

Three lines at the top center:

1. Place: `МЕСТО 4 / 8`.
2. Overtake or lead (8.2):

   | Situation                     | Text                            |
   |-------------------------------|---------------------------------|
   | Not first                     | `До обгона Freeman: 3 убийства` |
   | First, ahead of second        | `Отрыв: 2 убийства`             |
   | First, tied with the next one | `Вровень`                       |
   | No opponents                  | `Ожидание соперников`           |

3. Kills on the current level (8.4.3), with a visible gap above it.

- Tied leaders both see `МЕСТО 1 / 8` and `Вровень`.
- A long name is cut to 20 characters plus `…`. Characters are counted in UTF-8 code points after
  color codes are stripped.
- `убийство` follows the plural rules of the client language (10.2).

Warmup:

| `phase` | Line 1                         | Lines 2 and 3, level bar |
|---------|--------------------------------|--------------------------|
| 1       | `РАЗМИНКА 0:15` (`phase_left`) | Hidden                   |
| 2       | `СТАРТ ЧЕРЕЗ 3` (`phase_left`) | Hidden                   |

The plugin updates `phase_left` every second, as its warmup timer already does.

#### 8.4.3 Kills on the current level

- Up to 10 kills needed: one dot per kill, filled for done, empty for left.
- More than 10: a rounded bar, as if the dots merged. The filled part shows the share of done
  kills, the `16 / 25` counter stands to the right.
- The bar has the same length as a row of 10 dots and is centered too, so the block doesn't jump
  when the form changes. The form is chosen per level.

#### 8.4.4 Level bar

- One cell per item of `levels`, left to right in level order, each with the weapon icon.
- Passed levels are filled, the current one is highlighted, the rest are dimmed. No captions.
- The bar stands on the line of the health and ammo numbers.
- If the cells don't fit, they shrink down to a minimum size. If they still don't fit, the bar
  shows a window around the current level and marks the hidden ends with `‹` and `›`.

#### 8.4.5 Look

- One color for everything, `hud_color` (default `255 160 0`, `#FFA000`), icons included.
- Text uses VGUI2 scheme fonts (new entries in `ClientScheme`), so Cyrillic and scaling work and
  the fonts can be changed in `.res` files.
- Positions and sizes live in `HudLayoutBase.res` and can be overridden in `HudLayout.res`.

## 9. Plugin API

### 9.1 Server interface

`IBugfixedServer` gets minor version 2. `SetPlayerScore` is documented as added in 1.1, but
`IBUGFIXEDSERVER_MINOR` is still 0, so this change sets it to 2. New methods are appended:

- `GetClientCaps(int idx)` returns the caps of a client;
- `GetScoreboard()` returns `bhl::IScoreboardExt *`, a separate interface whose methods take
  column and list keys, never ids. New methods are appended to it the same way.

`bugfixedapi_amxx` checks the interface version per feature instead of refusing the whole API.
On a server older than 1.2 the existing natives keep working, `bhl_sb_available()` returns false,
`bhl_get_client_caps()` returns 0 and the other `bhl_sb_*` natives log one error and do nothing.

### 9.2 Natives

```pawn
enum BhlClientCaps
{
	BHL_CAPS_NONE = 0,
	BHL_CAPS_SCOREBOARD_V1 = (1 << 0),
	BHL_CAPS_GUNGAME_HUD_V1 = (1 << 1)
};

enum BhlSbDisplay
{
	BHL_SB_TEXT = 0,
	BHL_SB_ICON = 1,
	BHL_SB_METER = 2,
	BHL_SB_BUILTIN = 3,
	BHL_SB_STEPS = 4
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
	BHL_SB_COL_PRIMARY = (1 << 3)
};

const BHL_SB_SORT_ASC = 0x80;

const BhlSbTarget:BHL_SB_GLOBAL = BhlSbTarget:0;
const BhlSbTarget:BHL_SB_INVALID = BhlSbTarget:-1;

stock BhlSbTarget:bhl_sb_player(idx)
{
	return BhlSbTarget:idx;
}

native BhlSbTarget:bhl_sb_team(const teamName[]);

native BhlClientCaps:bhl_get_client_caps(idx);
forward bhl_client_caps_changed(idx);

native bool:bhl_sb_available();
native bhl_sb_reset();
native bhl_sb_config(BhlSbHide:hide, const preset[] = "", const title[] = "", const tag[] = "");
native bool:bhl_sb_add_column(const key[], const title[], BhlSbDisplay:display = BHL_SB_TEXT,
                              width = 0, sort = 0, BhlSbColumnFlags:flags = BHL_SB_COL_NONE);

native bhl_sb_set_int(BhlSbTarget:target, const key[], value);
native bhl_sb_set_text(BhlSbTarget:target, const key[], const text[]);
native bhl_sb_set_icon(BhlSbTarget:target, const key[], const sprite[], const text[]);
native bhl_sb_set_progress(BhlSbTarget:target, const key[], cur, max);
native bhl_sb_clear(BhlSbTarget:target, const key[] = "");

native bool:bhl_sb_list_resize(const key[], count);
native bhl_sb_list_set_int(const key[], index, value);
native bhl_sb_list_set_text(const key[], index, const text[]);
native bhl_sb_list_set_icon(const key[], index, const sprite[], const text[]);
```

- Teams are addressed by name, the same name `get_user_team()` returns. `bhl_sb_team()` returns
  an opaque handle that stays valid until the map changes, or `BHL_SB_INVALID` for an unknown team.
- Values for players that are not connected are ignored.
- `sort` is a priority starting at 1; add `BHL_SB_SORT_ASC` for ascending order.
- A `BHL_SB_BUILTIN` column takes one of the keys from 4.3: `rank`, `frags`, `deaths`, `eff`,
  `steamid`, `ping`.
- `bhl_sb_list_resize()` creates the list on first use. Setting an item out of range is ignored.
- Setting a cell or an item to the value it already has sends nothing.

### 9.3 GunGame

The plugin keeps working without the module: `set_module_filter` and `set_native_filter` in
`plugin_natives` let it load, and every call is guarded by a flag. The `level * 100` frags hack
stays for old clients. A new per-player counter keeps the kills of the warmup and then of the
match for the `kills` column.

```pawn
Scoreboard_Setup()
{
	if (!g_bhlApi || !bhl_sb_available())
		return

	new bool:isTeamPlay = get_pcvar_num(cvar[CVAR_TEAMPLAY]) == 1

	bhl_sb_reset()
	bhl_sb_config(BHL_SB_HIDE_FRAGS | BHL_SB_HIDE_DEATHS | BHL_SB_HIDE_EFF | BHL_SB_HIDE_STEAMID,
	              "gungame", "", isTeamPlay ? "TEAM" : "FFA")
	bhl_sb_add_column("rank", "", BHL_SB_BUILTIN)
	Scoreboard_SetColumns(g_isWarmup)
	bhl_sb_add_column("phase", "", BHL_SB_TEXT, 0, 0, BHL_SB_COL_HIDDEN)
	bhl_sb_add_column("phase_left", "", BHL_SB_TEXT, 0, 0, BHL_SB_COL_HIDDEN)

	bhl_sb_list_resize("levels", g_levelsCount)

	for (new i; i < g_levelsCount; i++)
		bhl_sb_list_set_icon("levels", i, g_levelIcon[i], g_levelWeaponName[i])
}

// Called again with the other value when the warmup starts or ends; cells are kept
Scoreboard_SetColumns(bool:warmup)
{
	new BhlSbColumnFlags:levelFlags = warmup ? BHL_SB_COL_HIDDEN : BHL_SB_COL_NONE
	new BhlSbColumnFlags:killsFlags = warmup ? BHL_SB_COL_PRIMARY : BHL_SB_COL_HIDDEN

	bhl_sb_add_column("level", "#BHL_SB_Level", BHL_SB_STEPS, 60, 0, warmup ? BHL_SB_COL_HIDDEN : BHL_SB_COL_PRIMARY)
	bhl_sb_add_column("weapon", "#BHL_SB_Weapon", BHL_SB_ICON, 120, 0, levelFlags)
	bhl_sb_add_column("progress", "#BHL_SB_Progress", BHL_SB_METER, 90, 0, levelFlags)
	bhl_sb_add_column("kills", "#BHL_SB_Kills", BHL_SB_TEXT, 60, warmup ? 1 : 2, BHL_SB_COL_RIGHT | killsFlags)
	bhl_sb_add_column("score", "", BHL_SB_TEXT, 0, warmup ? 2 : 1, BHL_SB_COL_HIDDEN)
}

Scoreboard_UpdatePlayer(id)
{
	if (!g_bhlApi)
		return

	new level = playersData[id][PLAYER_CURRENTLEVEL]
	new BhlSbTarget:target = bhl_sb_player(id)

	bhl_sb_set_progress(target, "level", level + 1, g_levelsCount)
	bhl_sb_set_icon(target, "weapon", g_levelIcon[level], g_levelWeaponName[level])
	bhl_sb_set_progress(target, "progress", playersData[id][PLAYER_KILLS], playersData[id][PLAYER_NEEDKILLS])
	bhl_sb_set_int(target, "kills", playersData[id][PLAYER_TOTALKILLS])
	bhl_sb_set_int(target, "score", Player_GetScore(id))
}
```

Names of plugin variables and helpers in the example are illustrative.

| Plugin function          | Scoreboard call                                       |
|--------------------------|-------------------------------------------------------|
| `Enable_GunGame`         | `Scoreboard_Setup()`                                  |
| `Disable_GunGame`        | `bhl_sb_reset()`                                      |
| `Equip_PlayerWithWeapon` | `Scoreboard_UpdatePlayer(id)`                         |
| `On_PlayerKilled`        | `Scoreboard_UpdatePlayer(killer)`                     |
| `Update_TeamData`        | Teammates and `bhl_sb_team(teamName)` cells           |
| `WarmUp_Timer`           | `phase` 1 or 2 and `phase_left`; `phase` 0 at the end |
| `Scoreboard_SetPhase`    | Warmup or match columns when the phase changes        |
| `resetPlayersScore`      | `kills` 0 for everyone                                |
| `endgame`                | Final values for everyone, `phase` 3                  |
| Auto-save restore        | `Scoreboard_UpdatePlayer(id)`                         |

For clients with `GunGameHudV1` the plugin skips:

- the text informer (`Show_HudInformer`);
- `StatusIcon`;
- the warmup messages from `WarmUp_Timer`, which then go to every other player one by one
  instead of `show_hudmessage(0, …)`.

The winner message, the final table and chat notifications stay for everyone.

## 10. Localization

### 10.1 Tokens

The client adds tokens to `bugfixedhl_*.txt`:

| Token                | English                  | Russian                   |
|----------------------|--------------------------|---------------------------|
| `BHL_SB_Level`       | Level                    | Уровень                   |
| `BHL_SB_Weapon`      | Weapon                   | Оружие                    |
| `BHL_SB_Progress`    | Progress                 | Прогресс                  |
| `BHL_SB_Kills`       | Kills                    | Убийства                  |
| `BHL_GG_Final`       | FINAL                    | ФИНАЛ                     |
| `BHL_GG_LeaderFinal` | Leader on the last level | Лидер на последнем уровне |
| `BHL_GG_LeaderLine`  | %s1 · %s2 · %s3 to win   | %s1 · %s2 · %s3 до победы |
| `BHL_GG_Place`       | PLACE %s1 / %s2          | МЕСТО %s1 / %s2           |
| `BHL_GG_Overtake`    | To pass %s1: %s2         | До обгона %s1: %s2        |
| `BHL_GG_Lead`        | Lead: %s1                | Отрыв: %s1                |
| `BHL_GG_Tied`        | Tied                     | Вровень                   |
| `BHL_GG_Waiting`     | Waiting for opponents    | Ожидание соперников       |
| `BHL_GG_Warmup`      | WARMUP %s1               | РАЗМИНКА %s1              |
| `BHL_GG_StartIn`     | START IN %s1             | СТАРТ ЧЕРЕЗ %s1           |
| `BHL_GG_KillsOne`    | %s1 kill                 | %s1 убийство              |
| `BHL_GG_KillsFew`    | %s1 kills                | %s1 убийства              |
| `BHL_GG_KillsMany`   | %s1 kills                | %s1 убийств               |

### 10.2 Plural forms

The client picks the `One`, `Few` or `Many` token by the rule of its language:

| Language | One                         | Few                                 | Many  |
|----------|-----------------------------|-------------------------------------|-------|
| Russian  | n % 10 = 1 and n % 100 ≠ 11 | n % 10 in 2–4 and n % 100 not 12–14 | Other |
| English  | n = 1                       | —                                   | Other |

A language without its own rule uses the English one.

### 10.3 Weapon names

Names of standard HL weapons are the same in every language, as in the mockup. They are tokens
`BHL_Weapon_<sprite without d_>` with English text in every language file.

| Sprite         | Name     | Source   |
|----------------|----------|----------|
| `d_crowbar`    | Crowbar  | mockup   |
| `d_9mmhandgun` | Glock    | proposal |
| `d_357`        | .357     | mockup   |
| `d_9mmAR`      | MP5      | mockup   |
| `d_shotgun`    | Shotgun  | proposal |
| `d_crossbow`   | Crossbow | proposal |
| `d_bolt`       | Crossbow | proposal |
| `d_rpg_rocket` | RPG      | proposal |
| `d_gauss`      | Gauss    | mockup   |
| `d_egon`       | Egon     | proposal |
| `d_hornet`     | Hornet   | mockup   |
| `d_grenade`    | Grenade  | mockup   |
| `d_satchel`    | Satchel  | mockup   |
| `d_tripmine`   | Tripmine | proposal |
| `d_snark`      | Snark    | proposal |

## 11. Compatibility

Client and server:

| Client \ Server                     | Old / AG / Valve | New, no schema | New, schema active |
|-------------------------------------|------------------|----------------|--------------------|
| Old (BugfixedHL, Valve, AG, OpenAG) | as now           | as now         | as now             |
| New                                 | standard         | standard       | custom             |
| New, `hud_scoreboard_custom 0`      | standard         | standard       | standard           |
| New, `_bhl` not in userinfo         | standard         | standard       | custom via query ¹ |

¹ If the client passes the p47 and LAN checks.

GunGame on a server with the new plugin:

| Client                         | Scoreboard | HUD                                             |
|--------------------------------|------------|-------------------------------------------------|
| Old                            | standard   | Plugin informer, `StatusIcon`, warmup texts     |
| New, `hud_gungame 1`           | GunGame    | Widget; plugin texts off except winner and chat |
| New, `hud_gungame 0`           | GunGame    | Plugin informer, `StatusIcon`, warmup texts     |
| New, `hud_scoreboard_custom 0` | standard   | Plugin informer, `StatusIcon`, warmup texts     |

Module and server DLL:

| `bugfixedapi_amxx` \ `hl.so` | API 1.0 / 1.1                   | API 1.2    |
|------------------------------|---------------------------------|------------|
| Old module                   | as now                          | as now     |
| New module                   | old natives work, no `bhl_sb_*` | everything |
| No module                    | GunGame works as now            | same       |

## 12. Test plan

Capabilities:

- From a second client, the first client's userinfo has no `_bhl` (keys with `_` are not forwarded).
- Fill the userinfo with `setinfo` keys: `_bhl` doesn't fit, `bhl_caps` shows source `Query`, the
  custom scoreboard still works.
- `setinfo` during a game reaches `ClientUserInfoChanged`; `hud_scoreboard_custom 0/1` and
  `hud_gungame 0/1` switch the scoreboard and the plugin texts both ways.
- Writing the same `_bhl` value again doesn't cause `ClientUserInfoChanged` spam.

Protocol and server:

- Every cell of the compatibility tables in section 11.
- Map change, reconnect, joining mid-game, GunGame on and off, teamplay.
- 32 players with bots: the full sync never triggers `Reliable channel overflowed`.
- The full sync arrives after the client's `VidInit` and nothing is lost.
- Malformed messages: the client doesn't crash (checked offline by feeding the parser).

GunGame scoreboard:

- 640×480, 1280×720 and 1920×1080: the table has the size of the standard one and the columns fit.
- 12 and 32 players: compact spacing, then scrolling.
- 9, 14 and 24 levels in the level strip; dots without a counter with 1–10 kills, the bar with
  the counter with more.
- Warmup: level, weapon and progress are hidden, kills and places by kills are shown; the match
  columns come back when the match starts.
- Ties: shared places, the banner with several leaders on the last level; a long server name
  next to the banner.
- Teamplay with 2 and 4 teams: team headers, players by kills, spectators at the bottom.
- Weapon icons in the scoreboard and the level bar with the OpenGL and the software renderer.
- A weapon with a non-standard sprite shows the name from the config.

GunGame HUD:

- 640×480, 1280×720 and 1920×1080 with 9, 12 and 24 levels: cells shrink, then the window with
  `‹ ›` appears.
- Warmup → countdown → game → win: the block changes, the plugin texts are not duplicated, the
  widget hides after the win.
- Ties, a single player on the server, teamplay with 2 and 4 teams.
- Spectator in first-person and free modes.
- Kills per level ≤ 10 and > 10 (teamplay): dots and bar have the same length, the block doesn't jump.
- Plural forms for 1, 2, 5, 11, 12, 21, 22, 25.
- The speedometer and `hud_jumpspeed` in bottom mode don't overlap the level bar; the status bar
  with names doesn't overlap it either.

## 13. Open questions

1. Are the opt-out settings needed, and are `hud_scoreboard_custom` and `hud_gungame` good names?
2. Icon tint in the scoreboard: the row text color or the HUD color?
3. Color codes in `Text` values: strip them in version 1, or draw them like in player names?
4. Exact sizes and colors of the HUD widget from the mockup: dots, bar, level cells and their
   minimum.
5. In teamplay, should player rows repeat the team level, weapon and progress, or leave them empty?
6. Weapon names marked as proposals in 10.3.
