# ERNativeUI API v1.0 snapshot provenance

These files are immutable copies of the public SDK released as
`ERNativeUI-v1.0.0`.

| Property | Value |
|---|---|
| Git tag | `ERNativeUI-v1.0.0` |
| Commit | `668f2f09ab3a175969db07a7c7063761ea2906f1` |
| `erui.h` Git blob | `d6503fabcd8f7265ce904ba492b26ac8ab562f85` |
| `ERNativeUI.hpp` Git blob | `1fe27782e3ea24d1e1f7ba7a12f50562fa19d1ce` |
| `erui.h` SHA-256 | `43c66ef69ebe5877a9b16d01d0692372917fa4d61b3faf80c9f36eeae20c0edf` |
| `ERNativeUI.hpp` SHA-256 | `4cd4fa5d9ea204e36d12a636d45a7c2b676e30fe2920578da5d87e849647847c` |

The Git identities can be independently verified with:

```text
git rev-list -n 1 ERNativeUI-v1.0.0
git rev-parse ERNativeUI-v1.0.0:include/ernativeui/erui.h
git rev-parse ERNativeUI-v1.0.0:include/ernativeui/ERNativeUI.hpp
```

Do not edit these headers after they are committed. A compatibility failure
must be fixed in the current host or treated as a new major API contract.
