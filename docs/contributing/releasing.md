# Releasing ERNativeUI

ERNativeUI uses Release Please, Semantic Versioning, Conventional Commits, and
GitHub Actions. The normal release path does not require manually editing the
project version, `CHANGELOG.md`, a tag, or a GitHub Release.

ERNativeUI **1.3.0** is the first project release that packages the finished
ERUI **API 1.3** contract. Project and API versions track different things and
can diverge:

- `1.3.0` is a project, package, tag, and changelog version.
- `1.3` is a runtime C function-table contract requested through
  `ERUI_GetApi` and by `erui::connect()`.

Release Please advances the project version only. It does not choose, freeze,
or validate an ERUI API version. See [Versioning](../versioning.md) for the full
contract.

Within a host release-major line, compatibility is cumulative: release
`X.Y.Z` must retain every previously published ERUI API `X.W` from that same
line. Removing one requires a host major-version change. A new host major may
drop earlier-major APIs unless its release notes explicitly retain them.

## Stable releases and API alphas

This guide describes stable releases from `main`. Unreleased API development
uses a separate integration branch and may publish clearly marked GitHub
prereleases for opt-in testing. GitHub marks those releases **Pre-release** and
keeps the current stable release marked **Latest**. The permanent
`/releases/latest` link therefore continues to serve stable users.

An alpha must not update the stable manifest, changelog, project version,
stable release tags, or Nexus file. It is produced by the manual **Publish API
alpha** workflow, not by Release Please. The workflow is selected explicitly
from an `api-1.x` branch, calculates its own next alpha ordinal, rebuilds and
tests that exact branch HEAD, and publishes a runtime-only package with exact
commit provenance.

Read the [alpha build policy](alpha-builds.md) for branch, provenance,
automatic numbering, generated notes, deletion, testing, security, and
promotion rules. The active public work is listed in the
[development roadmap](../roadmap/README.md).

### Alpha pipeline at a glance

```text
Reviewed Conventional Commits land on api-1.x
                |
                v
Windows x64 CI passes and the milestone is live-tested
                |
                v
Actions -> Publish API alpha -> Run workflow
Select api-1.x and confirm publication
                |
                v
Workflow derives the next alpha.N from existing prereleases
Build -> test -> package -> checksums -> generated notes
                |
                v
GitHub prerelease ERNativeUI-api-1.x-alpha.N
Stable ERNativeUI-vX.Y.Z remains Latest
```

Publication is never triggered merely by pushing or merging to `api-1.x`.
Deleting the newest test prerelease makes that ordinal reusable; the next run
also removes the exact orphan tag if GitHub retained it. Deleting an older
prerelease while a higher ordinal remains does not renumber published history.

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

After the 1.2.1 release, the release manifest records `1.2.1`. The API 1.3
feature commits therefore make Release Please propose project release
`1.3.0`. Confirm the calculated version in the release PR instead of editing
the manifest yourself. It is expected that the feature branch still shows
`1.2.1` in `CMakeLists.txt`; the generated release PR advances it.
If an intentional version override is ever necessary, use a dedicated
Conventional Commit with a `Release-As: X.Y.Z` footer; do not use that override
to make the project version mirror an API version.

### Maintain the What's New showcase

What's New is a curated, visual discovery path. It is not generated by Release
Please and must not duplicate the changelog, migration instructions, or API
reference. Prepare it in ordinary feature commits before the release PR.

For every release that introduces a new ERUI API or substantial visible
features:

1. Add a release-scoped page under `docs/whats-new/` and prepend it to
   `docs/whats-new/README.md`. State both the project release and the ERUI API
   first published by that release.
2. Use committed screenshots or animations from `docs/assets/images`, with
   descriptive alternative text. Keep the page focused on visible outcomes;
   link to task guides and the migration guide instead of copying code or
   contracts.
3. Include a new-examples section when the release adds examples, and mention
   an existing example only when it gained a meaningful mode for the featured
   work.
4. Update the version-specific What's New navigation link and showcase block
   in the root `README.md`, plus the showcase block in `docs/README.md`. Use
   the shared red badge at `docs/assets/images/badges/new.svg`, and move it to
   the current release instead of leaving it on the previous README cards.
5. Link the new page to the permanent archive and, when one exists, the
   preceding What's New page. Never delete or rewrite an older showcase except
   to correct a factual error or broken link.

A maintenance-only patch with no substantial visible change can keep the
current showcase instead of duplicating it. Record patch details in the
changelog and GitHub Release notes. Relative repository links are preferred:
when this work lands before the release PR is merged, the release tag preserves
the complete page, images, and linked documentation automatically.

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

The preset builds the host, examples, developer GFX tooling, and tests in
`build/preset-release`. A successful build also stages a Mod Engine 2-ready
tree in `build/preset-release/deploy/Release`; `build.bat` then installs the
SDK/runtime tree under `dist/ERNativeUI`. Developer tooling is built and tested
from source but is not installed into either public binary package.

Before releasing, also review:

- the [validation checklist](../../VALIDATION.md);
- the [API 1.0 to 1.1 migration guide](../migrations/1.0-to-1.1.md);
- the [API 1.1 to 1.2 migration guide](../migrations/1.1-to-1.2.md);
- the [API 1.2 to 1.3 migration guide](../migrations/1.2-to-1.3.md);
- the [feature matrix](../features.md) and compatibility claims;
- the current [What's New showcase](../whats-new/README.md), including every
  image, guide, migration, example, and archive link from both README entry
  points;
- the frozen API 1.0, 1.1 and 1.2 client tests and the current API 1.3 C/C++
  contract tests included in the normal test suite;
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

Release Please creates or refreshes one release PR. For the 1.3.0 release,
verify that it:

- changes `project(ERNativeUI VERSION ...)` in `CMakeLists.txt` to `1.3.0`;
- changes the manifest from `1.2.1` to `1.3.0`;
- updates `CHANGELOG.md` with the intended changes;
- presents custom tabs, title-screen text, runtime GFX completion, and
  configurable System tab slots as API 1.3 features without claiming unrelated
  research targets as released;
- includes the current What's New page and archive row, and points both the
  root and documentation READMEs at it with `[NEW]` markers only on the current
  release;
- records Elden Ring 1.17.2 / executable 2.7.2.0 support accurately;
- retains the frozen API 1.0, API 1.1, and API 1.2 contracts.

Do not merge the release PR until the `Windows x64` CI job passes and the
generated changelog accurately describes the release.

## 4. Configure GitHub once per repository

Under **Settings -> Actions -> General**, give workflows read and write
permission and allow GitHub Actions to create and approve pull requests.
Protect `main` with the `Windows x64` CI job as a required check.

Protect API integration branches with a matching `api-*` ruleset that requires
pull-request review and the same `Windows x64` check. The manual alpha workflow
has narrowly scoped `contents: write` permission because it creates a tag and
GitHub prerelease. GitHub executes the workflow definition from the branch
chosen in the **Run workflow** dialog, so never dispatch it from an untrusted
branch and review changes to alpha release infrastructure before merging them.

The Release Please workflow uses the Actions secret `RELEASE_PLEASE_TOKEN`
when present and falls back to the repository `GITHUB_TOKEN`. A fine-grained
personal access token or GitHub App token is recommended for
`RELEASE_PLEASE_TOKEN`; give it access to this repository's Contents, Issues,
and Pull requests with read/write permission.

GitHub generally does not start new workflows for events created by the
default `GITHUB_TOKEN`. Without the separate token, Release Please can still
create a release PR, but its pull-request CI may not start automatically. That
can conflict with a required CI rule. The dedicated token lets the generated
PR follow the same CI path as an ordinary PR.

No separate secret is needed merely to attach the Windows archive: the package
job uses its scoped `GITHUB_TOKEN` to upload to the GitHub Release.

## 5. Merge and verify the GitHub Release

Merging the 1.3.0 release PR causes a later Release Please run to create the
`ERNativeUI-v1.3.0` tag and GitHub Release. The component-prefixed form matches
the existing `ERNativeUI-v1.0.0` release and the current manifest
configuration. The workflow's Windows package job then:

1. checks out the exact released tag;
2. configures, builds, and tests with `windows-release`;
3. runs `tools/package_release.ps1` without `-IncludeNexus`;
4. uploads `ERNativeUI-1.3.0-windows-x64.zip` as a workflow artifact;
5. attaches that same single ZIP to the GitHub Release.

Verify the tag and release point to the release-PR merge commit, the release
notes are correct, and the GitHub Release has exactly the expected Windows
asset. If packaging fails after the release exists, correct the cause and use
**Re-run failed jobs**. The job checks out the release tag, so rerunning it does
not silently package newer `main` content.

Open the What's New page from the exact release tag and verify that its images
and guide, migration, example, archive, and any previous-page links render
there.
For release `X.Y.Z`, the expected GitHub path is
`blob/ERNativeUI-vX.Y.Z/docs/whats-new/X.Y.Z.md`. After publication, make the
corresponding archive row on `main` point to that tagged page so older entries
always open their immutable release documentation.

ERNativeUI 1.3.0 is the one retrospective exception: its What's New system was
added after `ERNativeUI-v1.3.0` already existed. Its showcase therefore lives
on `main`, with release-specific images and documentation pinned to the
existing tag. Never move or rewrite the published tag to insert the page.

The official workflow uses `tools/package_release.ps1`; CPack configuration is
also present for conventional local packaging, but it is not the GitHub
release-asset path.

## 6. Inspect or reproduce the Windows package locally

After the Release build succeeds, create the same kind of complete
runtime/SDK archive with:

```powershell
.\tools\package_release.ps1 `
  -BuildDirectory build/preset-release `
  -Version 1.3.0
```

The script installs the built tree and creates:

```text
dist/release/
|-- ERNativeUI-1.3.0-windows-x64.zip
`-- SHA256SUMS.txt
```

The Windows archive includes the runtime, public C and C++ headers, examples,
CMake package files, configuration and locales, licenses, third-party notices,
and a version-pinned link to the online documentation. It deliberately excludes
the developer-only GFX patcher and loose replacement GFX files, and does not
duplicate the repository's screenshot-heavy documentation tree.

The script removes and recreates the complete `dist/release` directory on each
run. Move any files you need to preserve before invoking it. Its `-Version`
argument does not rewrite the CMake project. It must match the configured
stable build identity, and the installed DLL must contain that identity and no
alpha identity; the script rejects a mismatched or stale build.

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
  -Version 1.3.0 `
  -IncludeNexus
```

This creates:

```text
dist/release/
|-- ERNativeUI-1.3.0-windows-x64.zip
|-- ERNativeUI-1.3.0-nexus.zip
`-- SHA256SUMS.txt
```

The Nexus ZIP is flattened for direct Mod Engine 2 use. It contains the host
DLL and default INI, locales, example DLLs, license and third-party notices.
Runtime transformation adapts the settings movies loaded by the game, so the
package does not install loose `menu` GFX overrides. Its `README.md` comes from
[`assets/NEXUS_README.md`](https://github.com/Flammrock/ERNativeUI/blob/main/assets/NEXUS_README.md): this is a concise, standalone player guide whose
documentation and support links are absolute. It does not contain the SDK
headers, CMake package, developer tools, version-pinned SDK documentation link,
or source examples found in the Windows archive.

The public Nexus description is maintained separately in
[`assets/NEXUS_PRESENTATION.bbcode`](https://github.com/Flammrock/ERNativeUI/blob/main/assets/NEXUS_PRESENTATION.bbcode).
It is a paste-ready source document for the Nexus **Description** editor in
BBCode mode; the packaging script does not copy it into either archive. Prefer
full-size `staticdelivery.nexusmods.com` URLs from the mod's Author images
gallery. A release-specific image may instead use an immutable
`raw.githubusercontent.com` URL pinned to the published release tag; never use
a moving branch for an embedded image. Keep hosted images published, avoid
gallery thumbnail URLs, and preview every image in the Nexus editor before
saving. Update version-specific compatibility wording whenever a new file is
uploaded.

Before uploading, extract the Nexus ZIP into a temporary directory and verify
its layout, hashes, DLL file versions, player README links, game-build
compatibility statement, description, screenshots, credits, permissions, and
AI disclosure fields. Confirm that repository README/SDK links were not
accidentally copied into this intentionally small package.
Upload `ERNativeUI-1.3.0-nexus.zip` manually; do not attach it to the GitHub
Release.

## 8. After publication

- Test a fresh Mod Engine 2 installation using the published Nexus ZIP.
- Test an SDK consumer using the published Windows ZIP.
- Confirm clients built against APIs 1.0, 1.1, and 1.2 still register with the
  1.3.0 host and a current client negotiates API 1.3. Include previously built
  binaries and clients intentionally built from every frozen SDK.
- Record any newly verified game or mod compatibility in the compatibility
  guide through a normal follow-up change.
- Retain the published What's New page permanently. For the next feature
  release, add a new page, link it to the archive and previous page, update the
  two README entry points, and move the red `[NEW]` marker forward.
- Keep `.release-please-manifest.json` at the version written by Release
  Please. Do not manually advance it for ordinary development.

For the next release, continue landing Conventional Commits. Release Please
will update the same release PR until it is merged, then calculate subsequent
project releases from the commits after the new tag. A project patch release
can retain ERUI API 1.3 unchanged; a new API contract requires its own design,
compatibility fixtures, migration guide, and explicit version decision.

## 9. Freeze a newly published API contract

Release Please versions packages; it does not create an immutable API fixture.
When a release is the first one to publish a new ERUI API version, freeze that
contract immediately after its release tag exists and before development can
change the public headers again. A later package release that keeps the same
API does not need another snapshot.

For API 1.3, use `tests/abi/releases/v1_2` as the structural example:

1. After the release exists, create `tests/abi/releases/v1_3` and copy the two
   public headers from the clean `ERNativeUI-v1.3.0` tag, byte-for-byte—not
   from a later `main`.
2. Add `ORIGIN.md` recording the release tag, commit, both header Git blob IDs,
   and both SHA-256 values.
3. Add `release.cmake` recording token `v1_3`, the encoded API version, frozen
   table-prefix size, C/C++ standards, and the same lowercase header hashes.
4. Add the release-owned layout, strict-C, C++ wrapper, contract-host,
   frozen-client-to-current-host, and any version-specific lifecycle tests.
   Cover both compatibility directions and host-to-client callbacks.
5. Append `erui_add_abi_release(v1_3)` to
   `tests/abi/releases/CMakeLists.txt` without removing or changing the
   `v1_0`, `v1_1`, or `v1_2` entries.
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
