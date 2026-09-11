# Releasing ERNativeUI

ERNativeUI uses Release Please, Semantic Versioning, Conventional Commits, and
GitHub Actions. The normal release path does not require manually editing the
project version, `CHANGELOG.md`, a tag, or a GitHub Release.

ERNativeUI **1.1.0** first shipped the finished ERUI **API 1.1** contract. The
Elden Ring 2.7.1 compatibility work targets project release **1.1.1** while
retaining that same API. Project and API versions track different things and
can diverge:

- `1.1.0` and `1.1.1` are project, package, tag, and changelog versions.
- `1.1` is the runtime C function-table contract requested through
  `ERUI_GetApi` and by `erui::connect()`.

Release Please advances the project version only. It does not choose, freeze,
or validate an ERUI API version. See [Versioning](../versioning.md) for the full
contract.

Within a host release-major line, compatibility is cumulative: release
`X.Y.Z` must retain every previously published ERUI API `X.W` from that same
line. Removing one requires a host major-version change. A new host major may
drop earlier-major APIs unless its release notes explicitly retain them.

## Release pipeline at a glance

```text
Conventional commits land on main
                |
                v
Release Please opens or updates one release PR
  - CMakeLists.txt project version
  - .release-please-manifest.json
  - CHANGELOG.md
                |
                v
Review the release PR and require Windows x64 CI
                |
                v
Merge the release PR
  - tag ERNativeUI-vX.Y.Z
  - create the GitHub Release
                |
                v
GitHub Actions builds/tests the tag and attaches
ERNativeUI-X.Y.Z-windows-x64.zip

Separately, from the same tag:
tools/package_release.ps1 -IncludeNexus
                |
                v
Review and upload ERNativeUI-X.Y.Z-nexus.zip manually
```

## 1. Prepare releasable commits

Use Conventional Commit subjects for changes merged into `main`:

```text
fix: restore dialog confirmation input
feat: add native input bindings
docs: explain native address discovery
```

The relevant release rules are:

- `fix:` requests a patch release.
- `feat:` requests a minor release.
- A `!` after the type or scope, together with an explanatory
  `BREAKING CHANGE:` footer, requests a major release.
- Documentation, test, build, and maintenance commits are recorded as
  appropriate but do not normally request a version bump on their own.

When a pull request is squash-merged, its title becomes the commit subject.
Keep that title in Conventional Commit form.

Before the compatibility fix lands, the release manifest records `1.1.0`. A
`fix:` subject therefore makes Release Please propose project release `1.1.1`;
the public ERUI API remains `1.1`. Confirm the calculated version in the
release PR instead of editing the manifest yourself. It is expected that the
fix branch still shows `1.1.0` in `CMakeLists.txt`; the generated release PR
advances it.
If an intentional version override is ever necessary, use a dedicated
Conventional Commit with a `Release-As: X.Y.Z` footer; do not use that override
to make the project version mirror an API version.

## 2. Run the local release checks

Requirements are CMake 3.28 or newer, Visual Studio 2022 or newer with the x64
C++ tools, and Git.

From the repository root, the convenient full check is:

```powershell
.\build.bat
```

It performs a clean-first Release build, runs the test suite, and refreshes the
install tree only after the tests pass. The equivalent commands are:

```powershell
cmake --preset windows-release
cmake --build --preset windows-release --clean-first
ctest --preset windows-release
cmake --install build/preset-release --config Release --prefix dist/ERNativeUI
```

The preset builds the host, examples, GFX patcher, and tests in
`build/preset-release`. A successful build also stages a Mod Engine 2-ready
tree in `build/preset-release/deploy/Release`; `build.bat` then installs the
SDK/runtime tree under `dist/ERNativeUI`.

Before releasing, also review:

- the [validation checklist](../../VALIDATION.md);
- the [API 1.0 to 1.1 migration guide](../migrations/1.0-to-1.1.md);
- the [feature matrix](../features.md) and compatibility claims;
- the frozen API 1.0 client tests and the current API 1.1 C/C++ contract tests
  included in the normal test suite;
- the manual MinGW-w64-to-MSVC runtime gate in
  [VALIDATION.md](../../VALIDATION.md#cross-toolchain-client-validation).

GitHub CI repeats configure, build, and test. It also installs the project into
an isolated prefix, configures a strict-C consumer against the installed CMake
package, builds and runs that consumer, and uploads the install tree as a CI
artifact.

The normal CI job builds both sides of the ABI tests with MSVC. It does not
prove that a MinGW-built client can negotiate with the MSVC host or safely
receive a host-to-client callback. When MinGW compatibility is part of the
release claim, record a successful run of the manual cross-toolchain matrix
before merging the release PR. Compile-only GCC/G++ header checks do not
satisfy that gate.

## 3. Let Release Please prepare the release PR

Every push to `main` runs [the Release Please workflow](https://github.com/Flammrock/ERNativeUI/blob/main/.github/workflows/release-please.yml).
Its configuration is stored in
[`release-please-config.json`](https://github.com/Flammrock/ERNativeUI/blob/main/release-please-config.json), and
`.release-please-manifest.json` records the most recently released project
version.

Release Please creates or refreshes one release PR. For the 1.1.1
compatibility release, verify that it:

- changes `project(ERNativeUI VERSION ...)` in `CMakeLists.txt` to `1.1.1`;
- changes the manifest from `1.1.0` to `1.1.1`;
- updates `CHANGELOG.md` with the intended changes;
- does not present exploratory API 1.2 ideas as released features.

Do not merge the release PR until the `Windows x64` CI job passes and the
generated changelog accurately describes the release.

## 4. Configure GitHub once per repository

Under **Settings -> Actions -> General**, give workflows read and write
permission and allow GitHub Actions to create and approve pull requests.
Protect `main` with the `Windows x64` CI job as a required check.

The workflow uses the Actions secret `RELEASE_PLEASE_TOKEN` when present and
falls back to the repository `GITHUB_TOKEN`. A fine-grained personal access
token or GitHub App token is recommended for `RELEASE_PLEASE_TOKEN`; give it
access to this repository's Contents, Issues, and Pull requests with read/write
permission.

GitHub generally does not start new workflows for events created by the
default `GITHUB_TOKEN`. Without the separate token, Release Please can still
create a release PR, but its pull-request CI may not start automatically. That
can conflict with a required CI rule. The dedicated token lets the generated
PR follow the same CI path as an ordinary PR.

No separate secret is needed merely to attach the Windows archive: the package
job uses its scoped `GITHUB_TOKEN` to upload to the GitHub Release.

## 5. Merge and verify the GitHub Release

Merging the 1.1.1 release PR causes a later Release Please run to create the
`ERNativeUI-v1.1.1` tag and GitHub Release. The component-prefixed form matches
the existing `ERNativeUI-v1.0.0` release and the current manifest
configuration. The workflow's Windows package job then:

1. checks out the exact released tag;
2. configures, builds, and tests with `windows-release`;
3. runs `tools/package_release.ps1` without `-IncludeNexus`;
4. uploads `ERNativeUI-1.1.1-windows-x64.zip` as a workflow artifact;
5. attaches that same single ZIP to the GitHub Release.

Verify the tag and release point to the release-PR merge commit, the release
notes are correct, and the GitHub Release has exactly the expected Windows
asset. If packaging fails after the release exists, correct the cause and use
**Re-run failed jobs**. The job checks out the release tag, so rerunning it does
not silently package newer `main` content.

The official workflow uses `tools/package_release.ps1`; CPack configuration is
also present for conventional local packaging, but it is not the GitHub
release-asset path.

## 6. Inspect or reproduce the Windows package locally

After the Release build succeeds, create the same kind of complete
runtime/SDK archive with:

```powershell
.\tools\package_release.ps1 `
  -BuildDirectory build/preset-release `
  -Version 1.1.1
```

The script installs the built tree and creates:

```text
dist/release/
|-- ERNativeUI-1.1.1-windows-x64.zip
`-- SHA256SUMS.txt
```

The Windows archive includes the runtime, public C and C++ headers, examples,
documentation, GFX patcher, CMake package files, configuration/assets, license,
and third-party notices.

The script removes and recreates the complete `dist/release` directory on each
run. Move any files you need to preserve before invoking it. Its `-Version`
argument names the output; it does not rewrite or verify the CMake project
version, so use the exact released version.

## 7. Create the Nexus package locally

Nexus packaging and upload are deliberately not automated. Build the exact
published tag in a clean checkout or worktree, run the checks again, and then
request both local package variants:

```powershell
cmake --preset windows-release
cmake --build --preset windows-release --clean-first
ctest --preset windows-release

.\tools\package_release.ps1 `
  -BuildDirectory build/preset-release `
  -Version 1.1.1 `
  -IncludeNexus
```

This creates:

```text
dist/release/
|-- ERNativeUI-1.1.1-windows-x64.zip
|-- ERNativeUI-1.1.1-nexus.zip
`-- SHA256SUMS.txt
```

The Nexus ZIP is flattened for direct Mod Engine 2 use. It contains the host
DLL and default INI, locales, optional `menu` GFX assets, example DLLs, license
and third-party notices. Its `README.md` comes from
[`assets/NEXUS_README.md`](https://github.com/Flammrock/ERNativeUI/blob/main/assets/NEXUS_README.md): this is a concise, standalone player guide whose
documentation and support links are absolute. It does not contain the SDK
headers, CMake package, patcher executable, full documentation library, or
source examples found in the Windows archive.

The public Nexus description is maintained separately in
[`assets/NEXUS_PRESENTATION.bbcode`](https://github.com/Flammrock/ERNativeUI/blob/main/assets/NEXUS_PRESENTATION.bbcode).
It is a paste-ready source document for the Nexus **Description** editor in
BBCode mode; the packaging script does not copy it into either archive. Its
presentation images use full-size `staticdelivery.nexusmods.com` URLs from the
mod's Author images gallery. Keep those hosted images published, avoid gallery
thumbnail URLs, and preview the result on Nexus before saving. Update
version-specific compatibility wording whenever a new file is uploaded.

Before uploading, extract the Nexus ZIP into a temporary directory and verify
its layout, hashes, DLL file versions, player README links, game-build
compatibility statement, description, screenshots, credits, permissions, and
AI disclosure fields. Confirm that repository README/SDK links were not
accidentally copied into this intentionally small package.
Upload `ERNativeUI-1.1.1-nexus.zip` manually; do not attach it to the GitHub
Release.

## 8. After publication

- Test a fresh Mod Engine 2 installation using the published Nexus ZIP.
- Test an SDK consumer using the published Windows ZIP.
- Confirm clients built against API 1.0 still register with the 1.1.1 host and
  a current client negotiates API 1.1. This includes both a previously built
  binary and a client intentionally built from the frozen 1.0 SDK.
- Record any newly verified game or mod compatibility in the compatibility
  guide through a normal follow-up change.
- Keep `.release-please-manifest.json` at the version written by Release
  Please. Do not manually advance it for ordinary development.

For the next release, continue landing Conventional Commits. Release Please
will update the same release PR until it is merged, then calculate subsequent
project releases from the commits after the new tag. A project patch release
can retain ERUI API 1.1 unchanged; a new API contract requires its own design,
compatibility fixtures, migration guide, and explicit version decision.

## 9. Freeze a newly published API contract

Release Please versions packages; it does not create an immutable API fixture.
When a release is the first one to publish a new ERUI API version, freeze that
contract immediately after its release tag exists and before development can
change the public headers again. A later package release that keeps the same
API does not need another snapshot.

For API 1.1, use `tests/abi/releases/v1_0` as the structural example:

1. Create `tests/abi/releases/v1_1` and copy the two public headers from the
   clean `ERNativeUI-v1.1.0` tag, byte-for-byte—not from a later `main`.
2. Add `ORIGIN.md` recording the release tag, commit, both header Git blob IDs,
   and both SHA-256 values.
3. Add `release.cmake` recording token `v1_1`, the encoded API version, frozen
   table-prefix size, C/C++ standards, and the same lowercase header hashes.
4. Add the release-owned layout, strict-C, C++ wrapper, contract-host,
   frozen-client-to-current-host, and any version-specific lifecycle tests.
   Cover both compatibility directions and host-to-client callbacks.
5. Append `erui_add_abi_release(v1_1)` to
   `tests/abi/releases/CMakeLists.txt` without removing or changing the
   `v1_0` entry.
6. Run the complete root test suite and the cross-toolchain runtime gate, then
   commit the snapshot and provenance.

The detailed fixture responsibilities and verification commands are in the
[ABI test README](../../tests/abi/README.md#freeze-a-newly-released-api).
Every previous release directory remains immutable. Within the current host
release-major line, every earlier API from that line stays in the
frozen-client-to-current-host matrix; if one stops working, correct the host.
A new host major may remove prior-major APIs only after the release explicitly
documents that decision and adjusts the active runtime matrix. Never rewrite
frozen headers, provenance, or hashes to hide an incompatibility.
