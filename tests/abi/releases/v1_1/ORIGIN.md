# ERNativeUI API v1.1 snapshot provenance

These files are immutable copies of the public SDK released as
`ERNativeUI-v1.1.1`. This patch release is the final published API 1.1 SDK
and therefore freezes the complete 320-byte API 1.1 table.

| Property | Value |
|---|---|
| Git tag | `ERNativeUI-v1.1.1` |
| Commit | `d951da696fba156d0656b9fbb5205a075a041c03` |
| `erui.h` Git blob | `ca81542924895586cef4f82f7db7975d8c375853` |
| `ERNativeUI.hpp` Git blob | `4dc6928d2167bf8942b25158d42f23bbdd81204e` |
| `erui.h` SHA-256 | `05442eee2818bceadde83f7e8cdbe44862cc3aecbd597d28dd93ad0efa763437` |
| `ERNativeUI.hpp` SHA-256 | `3b35aeff7bd33c0f76d3583958c070f3eba9eb05a01d751a82d23b86f83ed512` |

The Git identities can be independently verified with:

```text
git rev-list -n 1 ERNativeUI-v1.1.1
git rev-parse ERNativeUI-v1.1.1:include/ernativeui/erui.h
git rev-parse ERNativeUI-v1.1.1:include/ernativeui/ERNativeUI.hpp
```

Do not edit these headers after they are committed. A compatibility failure
must be fixed in the current host or treated as a new major API contract.
