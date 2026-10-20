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
| `erui.h` SHA-256 | `a530cc51ab7331260494d25afd7dfb475a5faa3557ee2433f044d4b93b39ddfb` |
| `ERNativeUI.hpp` SHA-256 | `03f07daeacc7ef976a3f736ef4d678fabe3f6e39fc2069b28b0841fb6449c4b5` |

The Git identities can be independently verified with:

```text
git rev-list -n 1 ERNativeUI-v1.2.1
git rev-parse ERNativeUI-v1.2.1:include/ernativeui/erui.h
git rev-parse ERNativeUI-v1.2.1:include/ernativeui/ERNativeUI.hpp
```

Do not edit these headers after they are committed. A compatibility failure
must be fixed in the current host or treated as a new major API contract.
