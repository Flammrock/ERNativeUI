# API development alpha builds

Alpha builds let opt-in testers validate a working but incomplete slice of an
unreleased ERUI API. They are deliberately separate from stable releases:

- an alpha appears on GitHub Releases with a **Pre-release** badge;
- the current stable release keeps GitHub's **Latest** badge;
- `/releases/latest` continues to resolve to the stable release;
- no alpha is uploaded to the stable Nexus Files page; and
- an alpha is published only when a maintainer selects a useful milestone.

Alphas are not scheduled, nightly, or produced for every commit. Ordinary
development commits run CI but do not create public releases.

This guide uses `1.x` as a placeholder for whichever API minor is currently in
development. Replace `x` with the real number: API 1.4 uses `api-1.4`, while a
future API 1.7 would use `api-1.7`.

## Stable and alpha publication paths

The two publication paths never share version state:

| Stable release | API alpha |
|---|---|
| Built from `main` | Built from a selected `api-1.x` branch HEAD |
| Prepared by Release Please | Published by the manual **Publish API alpha** workflow |
| Tag `ERNativeUI-vX.Y.Z` | Tag `ERNativeUI-api-1.x-alpha.N` |
| GitHub **Latest** release | GitHub **Pre-release**, never latest |
| Complete runtime and SDK archive | Runtime-only test archive initially |
| May be published on Nexus | GitHub only |
| Stable API compatibility promise | No alpha-to-alpha compatibility promise |

Publishing an alpha does not edit `CMakeLists.txt`,
`.release-please-manifest.json`, `CHANGELOG.md`, or the stable project version.
Release Please continues to see only stable work merged to `main`.

## Branch model

The branches have different purposes:

- `main` remains stable and is the only Release Please branch;
- `api-1.x` contains unfinished API 1.x integration; and
- small feature branches open pull requests into `api-1.x`, not directly into
  `main`.

Create `api-1.x` from the current `main`. If `main` later receives a stable bug
fix, documentation correction, or game-build compatibility patch, carry that
stable work forward by merging `main` into the development branch:

```powershell
git switch api-1.x # Replace x with the API minor being developed.
git fetch origin
git merge origin/main
```

This direction does not publish unfinished work to `main`. It only prevents
the future API from losing fixes that already belong to the stable line.

When API 1.x is complete, one reviewed `api-1.x` to `main` pull request
promotes it through the protected stable release path. Do not rebase or
force-push commits already named by a public alpha: while an alpha is
published, its tag and source commit are immutable test evidence.

## What the current alpha package contains

The current automated alpha is intentionally **runtime-only**. It contains:

- `ERNativeUI.dll`, the default INI, and locales;
- the examples built from the same commit;
- `ALPHA.md` with the generated change list, exact provenance, warnings, known
  limitations, testing guidance, and rollback instructions;
- `BUILD-PROVENANCE.txt` and a payload SHA-256 manifest; and
- the project and third-party license files.

It deliberately omits public headers, source examples, the CMake SDK package,
stable package documentation links, and every Nexus payload. A draft API that
still changes between alphas must not look like a stable dependency that
another mod should redistribute.

The archive and release asset identify the API, alpha ordinal, and first 12
characters of the exact commit SHA:

```text
ERNativeUI-api-1.4-alpha.1-g1a2b3c4d5e6f-runtime-windows-x64.zip
```

An SDK alpha may be introduced later, after the draft API shape is coherent
enough to reject host/client revision mismatches safely. When that happens,
the policy and workflow must be updated explicitly. Until then, the presence
of draft headers on the source branch does not make the runtime archive an SDK
release.

## Before publishing

Publishing is a maintainer decision. The selected branch HEAD should satisfy
all of these gates:

- the required Windows x64 check is green for the exact commit;
- every frozen stable API compatibility fixture passes;
- the implemented milestone has focused automated tests;
- its documented live-test procedure has passed on the stated game build;
- incomplete and known-broken behavior is recorded clearly;
- installation and stable rollback have been rehearsed; and
- Conventional Commit subjects describe the changes since the previous
  stable release or alpha.

Do not publish merely because a branch exists. A milestone should be useful
enough that external testing can answer a specific question.

## Publish from GitHub Actions

The publication workflow is manual and has no `push` or `schedule` trigger:
[`.github/workflows/alpha-release.yml`](../../.github/workflows/alpha-release.yml)
orchestrates the build, the
[`tools/generate_alpha_release_notes.ps1`](../../tools/generate_alpha_release_notes.ps1)
generator, and the
[`tools/package_alpha.ps1`](../../tools/package_alpha.ps1) runtime packager.
GitHub exposes **Run workflow** only after the workflow exists on the default
branch. Land this release infrastructure on `main` first, then create or
update the `api-1.x` branch from that commit.

1. Open the repository's **Actions** tab.
2. Select **Publish API alpha**.
3. Select **Run workflow**.
4. In GitHub's branch selector, choose the intended `api-1.x` branch, such as
   `api-1.4`.
5. Check **Publish the selected api-* branch as its next alpha prerelease**.
6. Select **Run workflow** and wait for every job to finish.

The selected branch must match `api-<major>.<minor>`. The workflow rejects
`main`, arbitrary feature branches, detached tags, and malformed API branch
names.

For the selected branch HEAD, the workflow then:

1. records the exact full commit SHA before building;
2. derives the API target from the branch name;
3. calculates the next alpha ordinal from the GitHub prereleases that still
   exist for that API target;
4. configures a build identity such as
   `api-1.4-alpha.1+g1a2b3c4d5e6f`;
5. builds, runs the complete automated test suite, and installs the exact
   commit;
6. generates `ALPHA.md`, including categorized release notes and provenance;
7. verifies a clean checkout and confirms that `ERNativeUI.dll` contains the
   expected compiled alpha identity;
8. creates and verifies the runtime-only alpha archive and checksums, embedding
   that exact `ALPHA.md` in the archive;
9. creates tag `ERNativeUI-api-1.4-alpha.1`; and
10. publishes `ERUI API 1.4 Alpha 1 — incomplete` as a GitHub **Pre-release**
   with `latest` explicitly disabled.

The stable release remains **Latest** throughout this process. A failed build
or validation run creates no prerelease and consumes no alpha number.

### Automatic alpha numbering

The ordinal is not a workflow input. For one API target, the next number is
one greater than the highest matching GitHub prerelease that currently
exists:

```text
existing: alpha.1, alpha.2
next:     alpha.3
```

This behavior intentionally supports test publication and cleanup:

- deleting the newest prerelease makes its ordinal available again;
- if deleting the release leaves its Git tag behind, the workflow removes
  only that exact orphan alpha tag immediately before republishing it;
- the same cleanup recovers a tag reserved by an interrupted alpha run before
  its prerelease was published;
- deleting `alpha.2` while `alpha.3` still exists does not renumber history;
  the next publication remains `alpha.4`; and
- deleting every prerelease for the API makes the next publication
  `alpha.1` again.

Never move a tag while its prerelease still exists. The automatic orphan-tag
cleanup exists only to make a deliberately deleted newest prerelease
repeatable; it is not permission to rewrite published evidence.

### Generated release notes

Release notes use Conventional Commit subjects. Their fixed category order is
Breaking Changes, Features, Bug Fixes, Performance Improvements, Reverts,
Documentation, Code Refactoring, Tests, Build System, Continuous Integration,
Styles, Miscellaneous Chores, and Other Changes. Keep pull-request titles and
direct commit subjects meaningful: they become the concise change list testers
see.

The comparison range is automatic:

- `alpha.1` uses the latest stable release tag through the selected commit;
- later alphas use the highest existing alpha tag for the same API through the
  selected commit.

The selected comparison base must be an ancestor of the selected commit. The
workflow fails instead of publishing misleading notes if the API branch has
not inherited the current stable line; merge current `main` into the API branch
and run its validation again before retrying.

Every release body also begins with an incomplete-build warning and records
the source branch, exact SHA, comparison base, runtime-only scope, installation
warning, and stable rollback path. The generated `ALPHA.md` is both the exact
GitHub release body and the file embedded in the archive.

Release notes are a test summary, not a compatibility promise. A feature
listed in one alpha may change or be removed in a later alpha.

## Test an alpha

1. Keep an untouched copy of the current stable ERNativeUI package.
2. Open the named GitHub prerelease and confirm the **Pre-release** badge,
   expected API target, ordinal, and commit SHA.
3. Download the runtime ZIP and `SHA256SUMS.txt`, verify the archive hash, and
   read the embedded `ALPHA.md` before installation.
4. Replace the complete ERNativeUI runtime directory; do not mix alpha and
   stable files, and load exactly one `ERNativeUI.dll`.
5. Use the examples from the same archive. Do not rebuild or distribute an
   ordinary client mod against draft branch headers.
6. Play offline with Easy Anti-Cheat disabled and use the game-build matrix
   named by the corresponding roadmap milestone or test instructions. The
   automated release notes alone do not claim live-game validation.
7. Follow the milestone's focused test matrix before exploring unrelated
   behavior.
8. Restore the complete stable package after testing.

A useful report includes the API target, alpha ordinal and SHA, game executable
product version, loader, other DLLs and their load order, effective ERNativeUI
INI, logs, exact actions, expected result, actual result, and whether the
stable build reproduces the issue.

## Delete a test prerelease

Deleting an alpha does not affect the stable release or `main`. For a temporary
publication test:

1. open the alpha on **Releases**;
2. delete that prerelease;
3. leave the branch intact if another test publication will follow; and
4. run **Publish API alpha** again when ready.

If the deleted release was the newest alpha, the next workflow run reuses its
number and safely removes the exact orphan tag left by GitHub before recreating
it. For deliberate permanent cleanup, remove both release and tag, for example:

```powershell
gh release delete ERNativeUI-api-1.4-alpha.3 --cleanup-tag --yes
```

The workflow also uploads a separate Actions artifact as recoverable build
evidence. Deleting the GitHub prerelease does not delete that artifact. It
expires under the repository's artifact-retention policy; for an immediately
clean disposable test, delete the corresponding workflow run from the
**Actions** tab after deleting the prerelease.

Alpha cleanup is intentionally not tied to branch deletion. An accidental
branch deletion must not erase public test provenance. After the stable API is
published and its alpha history is no longer useful, clean up its prereleases
deliberately.

## Security and workflow limitations

The publishing job has repository `contents: write` permission because it must
create a tag and prerelease. Treat it as release infrastructure:

- protect `api-*` branches with pull-request review and required Windows x64
  CI;
- land release-workflow changes on `main` first, then create or update the API
  branch from that commit so its `.github/workflows` definitions remain
  identical to the reviewed default-branch versions;
- never run the workflow from an untrusted branch;
- review changes to `.github/workflows/alpha-release.yml`, the alpha packager,
  and release-note generator as carefully as stable release changes;
- remember that GitHub uses the workflow definition from the branch selected
  in the **Run workflow** dialog; and
- keep the branch selector on the exact reviewed API integration branch.

The workflow serializes publication per selected branch so two runs cannot
publish the same next ordinal at once. It packages the branch HEAD captured at
dispatch time, not a later commit pushed while the job is running. It rejects
a commit SHA that already has a published alpha for that API target; rerunning
an old publication is not a substitute for selecting a new milestone.

## Stable promotion

Promotion from `api-1.x` follows the ordinary protected release path only
after:

- the API table and capability semantics are frozen;
- clients from every previously published API pass against the new host;
- current strict-C and C++ tests pass;
- supported cross-toolchain claims are revalidated;
- required native lifecycle, focus, pagination, tab, mouse, controller, and
  close/reopen matrices pass in game;
- the previous-to-1.x migration guide, reference, feature matrix, examples,
  compatibility statements, and What's New page are complete; and
- the final `api-1.x` to `main` pull request passes `Windows x64`.

After merging, Release Please creates the normal stable release. Only that
stable tag freezes API 1.x as a published contract. Alpha tags and versions do
not update the stable manifest or changelog.

[Development roadmap](../roadmap/README.md) |
[Stable release procedure](releasing.md) | [Versioning](../versioning.md)
