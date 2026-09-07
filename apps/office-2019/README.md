# office-2019 — Microsoft Office 2019 (ProPlus 2019 Volume, C2R via ODT) silent install

Office 2019 has **no MSI edition** — the volume-license media is Click-to-Run, driven
by the **Office Deployment Tool** (ODT). This manifest runs a bundled ODT
`setup.exe /configure configuration.xml`, taking the payload from the mounted ISO
(`SourcePath="%ISO%"` in `configuration.xml`) so no internet download is needed.

`kind: "iso"`, `setupFrom: "app"` — mount ISO for payload → run **`apps\office-2019\setup.exe`**
(the ODT) → dismount. Fully silent (`Display Level="None"`, `AcceptEULA="TRUE"`).
No auto-reboot.

## What to put here (USB drive, not git)

| File | Where from |
|------|-----------|
| `setup.exe` | the **Office Deployment Tool**: <https://www.microsoft.com/download/details.aspx?id=49117> — run the downloaded exe, it extracts `setup.exe`; copy just that here. ~7 MB. |
| `office-2019-proplus2019volume.iso` | Office 2019 ProPlus 2019 **Volume** ISO from the VLSC (contains `Office\Data\...`). Rename to exactly this. |

`configuration.xml` is tracked in git. It targets `ProPlus2019Volume`, channel
`PerpetualVL2019`, and installs the OS language + en-us. The `PIDKEY` is Microsoft's
published GVLK for Office LTSC/2019 ProPlus Volume (KMS/ADBA) — replace only for MAK.

If the ODT `setup.exe` is missing, this app is **skipped with a clear message** — drop
it in and re-run.

## Notes

- Want to install a different language only? Remove the `<Language ID="MatchOS" />`
  line or change `en-us`.
- `<RemoveMSI />` uninstalls any older MSI Office first (2010–2016). Drop that line if
  you want them to coexist (not recommended).
- Detection: `HKLM\SOFTWARE\Microsoft\Office\ClickToRun\Configuration`.
- Activation: `<Property Name="AUTOACTIVATE" Value="1" />` tries KMS/ADBA at first run;
  otherwise use the `kmsoffline` app.
