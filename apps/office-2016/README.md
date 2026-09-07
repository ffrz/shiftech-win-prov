# office-2016 — Microsoft Office 2016 (Professional Plus VL, MSI) silent install

`kind: "iso"` — mount ISO → `setup.exe /config config.xml` (silent, no checkboxes,
no EULA) → dismount. No auto-reboot.

## What to put here (USB drive, not git)

| File | Where from |
|------|-----------|
| `office-2016-proplusvl-x86.iso` | Office 2016 **Professional Plus** volume-license ISO (the classic MSI edition from the VLSC, `proplus.ww\setup.exe`). Rename to exactly this. |

`config.xml` is tracked in git. `Product="ProPlusVL"` matches the VL MSI media;
`<PIDKEY>` only for MAK.

## Notes

- This is the **MSI** edition, NOT Click-to-Run / ODT. If your media has an
  `Office\Data\` folder and a top-level `setup.exe` but no `proplus.ww\`, it is C2R —
  use the `office-2019` manifest's approach (ODT + `configuration.xml`) instead.
- 64-bit media: rename to `office-2016-proplusvl-x86.iso` or edit `image` in `app.json`.
- Detection: `HKLM\SOFTWARE\...\Office\16.0\Common\InstallRoot` (2016 and 2019 share
  16.0 — if both could be present, prefer installing only one).
- Activation is separate (`kmsoffline` or your KMS host).
