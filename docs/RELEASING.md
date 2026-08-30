# Releases and versioning

ERNativeUI uses Release Please, Semantic Versioning, Conventional Commits, and
GitHub Actions. Maintainers do not manually edit the project version, tag a
release, or assemble ZIP files.

## Normal development

Use Conventional Commit subjects for commits merged into `main`:

```text
fix: restore dialog confirmation input
feat: add text input rows
docs: explain native address discovery
```

`fix:` requests a patch release, `feat:` requests a minor release, and a
breaking change requests a major release. Mark a breaking change with `!`, for
example `feat!: redesign provider registration`, and explain it in a
`BREAKING CHANGE:` footer. Documentation, tests, build, and maintenance commits
are recorded but do not independently force a version bump.

When GitHub squash-merges pull requests, the pull-request title becomes the
commit subject. Keep that title in Conventional Commit form.

## What Release Please does

Every push to `main` runs `.github/workflows/release-please.yml`. Release Please
reads commits since the latest release and creates or updates one release pull
request. That pull request contains:

- the next version in `.release-please-manifest.json`;
- the same project version in `CMakeLists.txt`;
- generated release notes in `CHANGELOG.md`.

Review that pull request like any other change. Merging it creates the `vX.Y.Z`
tag and GitHub Release. The same workflow then builds and tests Windows x64 and
attaches one asset to the GitHub Release:

- `ERNativeUI-X.Y.Z-windows-x64.zip`: complete runtime, SDK headers, examples,
  documentation, patcher, CMake package files, and license texts.

The Nexus package is deliberately not generated or uploaded by GitHub Actions.
Create it locally with `tools/package_release.ps1 -IncludeNexus`, then upload it
manually after reviewing its contents, description, screenshots, game-version
compatibility, and moderation fields.

## First stable release

The repository starts at development version `0.9.0`. To explicitly request
the first public `1.0.0` release, include this footer in a Conventional Commit
merged into `main`:

```text
chore: prepare first stable release

Release-As: 1.0.0
```

Do this only once. Subsequent versions are calculated from `fix:`, `feat:`, and
breaking-change commits.

## Repository settings

In GitHub, open **Settings -> Actions -> General** and give workflows read and
write permission. Enable the option allowing GitHub Actions to create and
approve pull requests. Protect `main`, require the Windows CI check, and review
the Release Please pull request before merging it.

GitHub deliberately prevents events created by the default `GITHUB_TOKEN` from
starting most other workflows. To make the Release Please pull request run CI,
create a fine-grained personal access token (or GitHub App token) with access
to this repository's contents and pull requests, save it as the Actions secret
`RELEASE_PLEASE_TOKEN`, and grant workflow access if the selected token type
requires it. The workflow falls back to `GITHUB_TOKEN`, but in that mode its
automatically created pull request may need its CI run started another way.

If release packaging fails after the GitHub Release is created, use GitHub's
**Re-run failed jobs** action after correcting any external cause. The package
job checks out the exact release tag, so it cannot silently package a later
commit.

Do not edit `.release-please-manifest.json` during ordinary development. It is
Release Please's record of the last released version.
