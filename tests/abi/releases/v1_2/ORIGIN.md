# ERNativeUI API v1.2 snapshot provenance

These files are immutable copies of the public SDK released as
`ERNativeUI-v1.2.1`. This patch release is the final published API 1.2 SDK
and therefore freezes the complete 368-byte API 1.2 table. The public headers
are byte-identical to the first API 1.2 release, `ERNativeUI-v1.2.0`.

| Property | Value |
|---|---|
| Git tag | `ERNativeUI-v1.2.1` |
| Commit | `24eede7613c3df32712ee2300073e1f1b0f43fdb` |
| `erui.h` Git blob | `28d941c5045a35b705032bbdfe471d86905f586e` |
| `ERNativeUI.hpp` Git blob | `d18c87cda115de69bf4436d33a8127a6d704fae1` |
| `erui.h` SHA-256 | `ca97015839cca40ea3989198c5c2d2c6331ce8f6b621c85e3d0b0bfc1ba37d65` |
| `ERNativeUI.hpp` SHA-256 | `b2e5dc17f36145bdecfe4bad29eb2714c08e97b98a874a3b1b9451cc261ca79b` |

The Git identities can be independently verified with:

```text
git rev-list -n 1 ERNativeUI-v1.2.1
git rev-parse ERNativeUI-v1.2.1:include/ernativeui/erui.h
git rev-parse ERNativeUI-v1.2.1:include/ernativeui/ERNativeUI.hpp
```

Do not edit these headers after they are committed. A compatibility failure
must be fixed in the current host or treated as a new major API contract.
