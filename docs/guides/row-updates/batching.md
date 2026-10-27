# Update several rows atomically

<table>
  <tr>
    <td align="center" valign="middle">
      <a href="../capabilities.md#what-a-capability-is"><img src="../../assets/images/badges/capabilities.svg" alt="Capabilities" width="112" height="22" align="top"></a>
    </td>
    <td valign="middle">
      <a href="../capabilities.md#updates"><code>updates</code></a>
    </td>
  </tr>
</table>

The strict-C API can submit several row updates in one atomic commit. This is
one `commit_updates` call containing multiple updates—not multiple commits
sent at the same time.

`updates` is the only capability required by batching itself. Individual
fields can require additional capabilities; for example, the availability
fields used below also require `row_availability`.

## C++ and C are intentionally different

| Surface | Commit scope | Atomic across rows? |
|---|---|---:|
| C++ `row.edit()...commit()` | One logical row; any number of staged fields on that row | No multi-row scope |
| Strict-C `commit_updates(...)` | One batch containing 1..4,096 update envelopes | Yes |

Several setters chained on one C++ draft are committed together for that row:

```cpp
const ERUI_Result result = online_strength.edit()
    .label(L"Online Strength (Unavailable)")
    .interactive(false)
    .availability_appearance(
        erui::AvailabilityAppearance::follow_interactivity)
    .commit();
```

However, two separate C++ `commit()` calls publish two separate logical
generations. API 1.4 deliberately has no C++ multi-row collector. A client
that requires cross-row atomicity uses the strict-C batch surface.

## Complete strict-C example

This example disables Matchmaking and Voice Chat together. `api` is the exact
API 1.4 table returned by `ERUI_GetApi`, `provider` is the committed provider
that owns both rows, and the row handles are the values returned while that
provider was registered.

```c
#include <ernativeui/erui.h>

ERUI_Result set_online_rows_enabled(
    const ERUI_Api* api,
    ERUI_ProviderHandle provider,
    ERUI_RowHandle matchmaking_row,
    ERUI_RowHandle voice_chat_row,
    int enabled)
{
    const ERUI_Capabilities required =
        ERUI_CAP_UPDATES | ERUI_CAP_ROW_AVAILABILITY;

    if (!api) {
        return ERUI_INVALID_ARGUMENT;
    }
    if (!api->commit_updates ||
        (api->capabilities & required) != required) {
        return ERUI_NOT_SUPPORTED;
    }

    ERUI_RowUpdateDesc rows[2] = {0};

    rows[0].size = (uint32_t)sizeof(rows[0]);
    rows[0].row = matchmaking_row;
    rows[0].fields =
        ERUI_ROW_UPDATE_FIELD_INTERACTIVE |
        ERUI_ROW_UPDATE_FIELD_AVAILABILITY_APPEARANCE;
    rows[0].interactive = enabled ? 1u : 0u;
    rows[0].availability_appearance =
        ERUI_AVAILABILITY_APPEARANCE_FOLLOW_INTERACTIVITY;

    rows[1] = rows[0];
    rows[1].row = voice_chat_row;

    ERUI_UpdateDesc updates[2] = {0};
    for (uint32_t index = 0; index < 2u; ++index) {
        updates[index].size = (uint32_t)sizeof(updates[index]);
        updates[index].kind = ERUI_UPDATE_KIND_ROW;
        updates[index].payload = &rows[index];
    }

    ERUI_UpdateBatchDesc batch = {0};
    batch.size = (uint32_t)sizeof(batch);
    batch.updates = updates;
    batch.update_count = 2u;
    batch.update_stride = (uint32_t)sizeof(updates[0]);

    return api->commit_updates(provider, &batch);
}
```

If either handle is invalid, belongs to another provider, or appears twice,
neither row changes. The same all-or-nothing rule applies to invalid fields,
invalid final visibility projections, and allocation failures.

## Batch contract

One `ERUI_UpdateBatchDesc` follows these rules:

- it contains between 1 and `ERUI_UPDATE_MAX_COUNT` (4,096) update envelopes;
- every row belongs to the provider passed to `commit_updates`;
- one row appears at most once in the batch;
- descriptor sizes and strides are exact;
- flags, reserved fields, and omitted-field payloads remain zero;
- `ERUI_CAP_UPDATES` is present;
- interaction or appearance fields additionally require
  `ERUI_CAP_ROW_AVAILABILITY`; and
- visibility fields additionally require `ERUI_CAP_ROW_VISIBILITY`.

The host validates and copies the complete batch before the call returns, so
the stack arrays and any text/span buffers may be released afterward. A
successful call publishes one logical UI generation. Native repaint or
reconstruction still occurs later at a safe boundary, so the operation is
safe from an ERNativeUI callback.

For individual C++ edits, return to the
[row-update foundation](README.md). For every result and recovery rule, see
[Error handling](../../reference/error-handling.md).
