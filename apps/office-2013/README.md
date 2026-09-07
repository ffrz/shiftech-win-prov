# office-2013 — Microsoft Office 2013 (ProPlus, volume/MSI) silent install

`kind: "iso"` — mount ISO → `setup.exe /config config.xml` (silent, no checkboxes,
no EULA) → dismount. No auto-reboot.

## What to put here (USB drive, not git)

| File | Where from |
|------|-----------|
| `office-2013-proplus-x86.iso` | Office 2013 ProPlus **volume-license** ISO (MSI). Rename to exactly this. |

`config.xml` is tracked in git. `<PIDKEY>` only needed for MAK.

## Notes

- Must be the MSI volume media (`\proplus.ww\config.xml` inside). Click-to-Run 2013
  (Office 365 installer) is a different mechanism — not this manifest.
- 64-bit media: put your file here and set `image` in `app.json` to its name, or just
  rename to `office-2013-proplus-x86.iso`.
- Detection: `HKLM\SOFTWARE\...\Office\15.0\Common\InstallRoot`.
- Activation is separate (`kmsoffline` app or your KMS host).
