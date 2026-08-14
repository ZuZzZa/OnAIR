# Releasing OnAIR firmware

OnAIR is a **public** project on the ZuZzZa hub. The site
(<https://zuzzza.github.io/#/p/onair>) reads this repo's **GitHub Releases live**
from the GitHub API — there is no hub sync, no `HUB_PUSH_TOKEN`, and no committed
binaries. You cut a well-named, published release and it appears on the site.

## Versioning & tags

- Scheme: `MAJOR.STEP.FIX` (see [CHANGELOG.md](../CHANGELOG.md)).
- Tags are **bare**, no `v` prefix (e.g. `2.0.1`) — matching existing tags.
- `src/version.h` is the source of truth. `FIRMWARE_VERSION` must equal the tag.
- `increment_version.py` auto-bumps the build number on **local** builds only; in
  CI it is skipped (`CI=true`), so the released binary reports exactly the tagged
  version. `release.yml` enforces `tag == FIRMWARE_VERSION` and fails otherwise.

## Release flow

1. **Set the version** in `src/version.h` (the three `FIRMWARE_VERSION_*` lines +
   the `FIRMWARE_VERSION` string), add a `## [x.y.z] — YYYY-MM-DD` section to
   `CHANGELOG.md`, commit, push to `main`.
2. **Tag it** and push the tag:
   ```bash
   git tag 2.0.1
   git push origin 2.0.1
   ```
   `release.yml` builds `d1_mini_lite` and creates a **draft** GitHub Release with:
   - `onair_2.0.1_d1mini.bin`      → shown as **OTA** (web `/update` image)
   - `onair_2.0.1_d1mini_full.bin` → shown as **FULL** (flash at `0x0`)

   On ESP8266 both files are the same `firmware.bin`; the two names just let the
   site offer both an OTA and a full-flash download with the right instructions.
3. **Review the draft** on GitHub (edit the auto-generated notes if you like).
4. **Publish** — Actions ▸ *Publish Release* ▸ Run workflow with the tag, or click
   *Publish release* in the GitHub UI. The public site only shows **published**
   releases; once live it appears at `#/p/onair` after a Pages refresh.

## Asset naming (why these names)

The hub parses filenames:
- `..._full.bin` → **FULL**, any other `.bin` → **OTA**.
- Board label: `d1mini` / `8266` / `8285` in the name → "Wemos D1 mini" (see the
  hub's `boardFromName()` in `index.html`).

Keep the `onair_<tag>_d1mini[...].bin` pattern so both parse correctly.

## Backfilling older versions (optional)

Existing tags (`1.1.4` … `2.0.0`) were built manually into `release_builds/`
(gitignored, not used by the site). To show them on the hub, create a published
GitHub Release per tag with a renamed asset, e.g.:
```bash
gh release create 2.0.0 \
  "release_builds/OnAIR_2.0.0.bin#onair_2.0.0_d1mini.bin" \
  --repo ZuZzZa/OnAIR --title 2.0.0 --notes-from-tag
```
Only needed if you want the full history downloadable from the site.

## Notes

- `build.yml` runs on every push/PR to keep `main` green before you tag.
- Restricted/agent sessions often can't push tags (git proxy 403); push tags from
  your own shell, or `git tag -f 2.0.1 <commit> && git push -f origin 2.0.1`.
