# office-2010 — Microsoft Office 2010 (ProPlus, volume) silent install

`kind: "iso"` — the provisioner mounts the ISO with `Mount-DiskImage`, runs
`setup.exe /config config.xml` (fully silent, no feature/checkbox screen, no EULA
prompt), then dismounts. No auto-reboot (V1 rule).

## What to put here (on the USB drive — not in git)

| File | Where from |
|------|-----------|
| `office-2010-proplus-x86.iso` | your Office 2010 ProPlus **volume-license** ISO. Rename it to exactly this. |

`config.xml` is already in this folder (tracked in git). Edit it only if you need a
MAK product key — uncomment the `<PIDKEY>` line.

## Notes

- ISO must be the **volume-license** ProPlus media (has `\ProPlus.WW\config.xml`).
  Retail/OEM media that boots an interactive "Enter your key" screen will not run silent.
- x64 media: rename the ISO to `office-2010-proplus-x86.iso` anyway (the id must match
  `app.json`'s `image`), or change `image` in `app.json` to your filename.
- Detection: `HKLM\SOFTWARE\...\Office\14.0\Common\InstallRoot`. If already present the
  app is reported `already_installed` and setup is not run.
- Activation is separate — use the `kmsoffline` app or your KMS host.
- Runs elevated (a real provisioning run already requires Administrator).
