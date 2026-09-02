# Application profiles

A profile is a named list of applications a technician can choose for provisioning.
Shipped in `profiles/`. Chosen with `--profile <name>`.

## Format

Canonical reference form is YAML; `ProfileLoader` currently loads the **`.json`**
equivalent (YAML support pending ADR-0002 in [DECISIONS.md](DECISIONS.md)). Both files are
shipped side by side and must stay in sync.

### YAML (reference)

```yaml
name: standard
description: Standard application profile

applications:
  - id: Google.Chrome
    required: true
  - id: 7zip.7zip
    required: true
  - id: VideoLAN.VLC
    required: false
  - id: SumatraPDF.SumatraPDF
    required: false
```

### JSON (loaded)

```json
{
  "name": "standard",
  "description": "Standard application profile",
  "applications": [
    { "id": "Google.Chrome", "required": true },
    { "id": "7zip.7zip", "required": true },
    { "id": "VideoLAN.VLC", "required": false },
    { "id": "SumatraPDF.SumatraPDF", "required": false }
  ]
}
```

## Fields

| Field | Type | Rules |
|-------|------|-------|
| `name` | string | required, matches the file stem |
| `description` | string | required |
| `applications[].id` | string | required, a valid **winget package Id** (`--exact`), unique within the profile |
| `applications[].required` | bool | default `false`. `required: true` failures make the run "SUCCESS WITH WARNINGS"; `required: false` failures are informational |

Unknown top-level or per-app keys ⇒ validation error (fail fast on a bad profile).

## Shipped profiles

| File | Purpose |
|------|---------|
| `profiles/standard.yaml` / `.json` | browser, archiver, media, PDF |
| `profiles/office.yaml` / `.json` | standard + office suite / PDF / comms |
| `profiles/technician.yaml` / `.json` | diagnostic & support tooling |
| `profiles/developer.yaml` / `.json` | standard + editors, git, runtimes |

Profiles are starting points — the exact package Ids should be verified against
`winget search` on a target machine and adjusted by the team.

## Behaviour (ApplicationProvider)

1. For each app: `isInstalled(id)` → if yes, record `AlreadyInstalled`, skip.
2. Else `install(id)` silently; capture output + exit code.
3. Retry once on a transient failure (network / source).
4. On failure: record `Failed` (+ `required` flag), continue to the next app.
5. Report totals: installed / already installed / failed.
