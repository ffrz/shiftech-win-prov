# Initial Development Prompt — Windows Provisioning Application

Anda adalah senior Windows systems engineer dan C++/Qt developer.

Bangun sebuah aplikasi Windows bernama **Shiftech Win Provisioner**, yaitu tool untuk membantu teknisi melakukan provisioning laptop/PC Windows 7/8/10/11 secara otomatis setelah instalasi Windows.

## Tujuan Utama

Aplikasi harus mengotomatisasi pekerjaan teknisi yang biasanya dilakukan secara manual:

1. Mendeteksi hardware/perangkat Windows.
2. Mengidentifikasi perangkat yang belum memiliki driver atau mengalami masalah driver.
3. Mencari driver berdasarkan Hardware ID.
4. Mendownload driver secara otomatis dari provider driver.
5. Menginstall driver secara otomatis.
6. Memverifikasi kembali status device setelah instalasi.
7. Melewati device yang drivernya tidak ditemukan tanpa menghentikan seluruh proses.
8. Menginstall aplikasi standar secara otomatis.
9. Menggunakan profile aplikasi sehingga teknisi dapat memilih jenis provisioning.
10. Menyediakan logging dan laporan hasil provisioning.

## Prinsip Penting

Jangan membuat aplikasi yang bergantung pada simulasi klik GUI seperti AutoIt.

Gunakan mekanisme native Windows jika memungkinkan.

Untuk driver, manfaatkan mekanisme Windows seperti:

* PnPUtil
* Windows SetupAPI / CfgMgr32 jika diperlukan
* Windows Device Installation APIs jika memang diperlukan

Untuk aplikasi, prioritaskan:

* WinGet
* installer silent/native command line
* package manager lain hanya jika diperlukan

Aplikasi harus berjalan dengan privilege Administrator ketika melakukan operasi yang membutuhkan elevation.

---

# Arsitektur

Pisahkan application core dari UI.

Struktur awal yang diinginkan:

```text
AmanahProvisioner/
├── src/
│   ├── core/
│   │   ├── hardware/
│   │   ├── drivers/
│   │   ├── applications/
│   │   ├── provisioning/
│   │   ├── profiles/
│   │   ├── logging/
│   │   └── system/
│   │
│   ├── cli/
│   └── gui/
│
├── profiles/
├── cache/
├── logs/
├── tests/
└── docs/
```

Gunakan dependency yang seminimal mungkin.

Gunakan **C++ modern** dan **Qt 6**.

Core engine harus sebisa mungkin dapat dijalankan tanpa GUI sehingga nantinya dapat digunakan oleh CLI, GUI, automation, atau service.

---

# Phase 1 — Hardware Detection

Buat modul:

```text
DeviceEnumerator
```

yang mampu mendapatkan informasi device dari Windows.

Minimal informasi:

```text
Device Name
Class
Manufacturer
Hardware IDs
Compatible IDs
Instance ID
Status
Problem Code
Driver Version
Driver Provider
Driver Date
```

Prioritaskan device yang:

* tidak memiliki driver
* memiliki problem code
* memiliki unknown device status
* membutuhkan driver

Gunakan native Windows API atau `pnputil` jika lebih praktis.

Jangan parsing output command secara berlebihan jika informasi dapat diperoleh secara lebih reliable melalui Windows API.

Buat abstraction:

```cpp
class DeviceEnumerator
{
public:
    std::vector<Device> enumerate();
};
```

Dengan model:

```cpp
struct Device
{
    std::string name;
    std::string className;
    std::vector<std::string> hardwareIds;
    std::vector<std::string> compatibleIds;
    std::string instanceId;

    DeviceStatus status;
    int problemCode;

    std::string driverVersion;
    std::string driverProvider;
};
```

---

# Phase 2 — Driver Resolution

Buat abstraction:

```text
DriverProvider
```

Jangan membuat DriverPack sebagai dependency langsung di seluruh aplikasi.

Gunakan interface:

```cpp
class DriverProvider
{
public:
    virtual DriverSearchResult search(
        const Device& device
    ) = 0;
};
```

Implementasi awal:

```text
DriverPackProvider
```

Provider harus menerima Hardware ID dan informasi Windows/architecture.

Contoh:

```text
PCI\VEN_10EC&DEV_8168
        ↓
DriverProvider
        ↓
Driver package
```

Driver provider harus dapat menghasilkan informasi:

```text
Driver name
Version
Provider
Supported OS
Architecture
Download URL
Package type
Checksum if available
```

### Important

Jangan mengasumsikan struktur website/API DriverPack tanpa melakukan investigasi terlebih dahulu.

Sebelum implementasi DriverPackProvider:

1. Pelajari bagaimana DriverPack menyediakan driver.
2. Tentukan apakah tersedia API resmi.
3. Tentukan mekanisme download yang stabil.
4. Tentukan apakah direct download dapat dilakukan secara reliable.
5. Perhatikan terms/licensing dan keamanan.
6. Jangan membuat scraper yang rapuh jika terdapat alternatif yang lebih reliable.

Jika belum ada mekanisme yang reliable, buat interface/provider mock terlebih dahulu dan dokumentasikan integration point.

---

# Phase 3 — Driver Download

Buat:

```text
DriverDownloader
```

Fitur:

* download file
* progress
* retry
* timeout
* checksum verification jika tersedia
* temporary file
* resume jika memungkinkan
* cache

Cache harus menggunakan identifier yang deterministic.

Contoh:

```text
cache/
└── drivers/
    └── <driver-package-id>/
        ├── package.zip
        └── metadata.json
```

Jika driver sudah ada di cache, jangan download ulang.

---

# Phase 4 — Driver Installation

Buat:

```text
DriverInstaller
```

Prioritaskan penggunaan:

```text
pnputil
```

Misalnya konsep:

```text
package
    ↓
extract
    ↓
find *.inf
    ↓
pnputil /add-driver ... /install
    ↓
verify
```

Installer harus:

* support multiple INF
* capture stdout/stderr
* capture exit code
* log command execution
* timeout
* report success/failure
* tidak menghentikan provisioning global ketika satu driver gagal

Jangan melakukan blind installation terhadap executable yang tidak diketahui.

---

# Phase 5 — Driver Verification

Setelah driver diinstall:

```text
enumerate devices again
```

Bandingkan:

```text
BEFORE
Unknown device
Problem Code 28

AFTER
Driver installed
Problem Code 0
```

Buat hasil:

```cpp
enum class DriverInstallStatus
{
    AlreadyInstalled,
    Installed,
    Failed,
    NotFound,
    Skipped,
    RequiresReboot
};
```

---

# Phase 6 — Application Provisioning

Buat abstraction:

```text
ApplicationProvider
```

Implementasi pertama:

```text
WinGetProvider
```

Profile aplikasi menggunakan file YAML atau JSON.

Contoh:

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

Profile lain:

```text
profiles/
├── standard.yaml
├── office.yaml
├── technician.yaml
└── developer.yaml
```

Aplikasi harus:

1. mendeteksi apakah package sudah terinstall
2. install jika belum
3. skip jika sudah ada
4. capture output
5. capture exit code
6. retry jika appropriate
7. melanjutkan aplikasi berikutnya jika satu package gagal

Jangan menggunakan GUI automation.

---

# Phase 7 — Provisioning Pipeline

Buat orchestration engine:

```text
ProvisioningEngine
```

Pipeline:

```text
START
  ↓
System Check
  ↓
Hardware Scan
  ↓
Driver Analysis
  ↓
Driver Resolution
  ↓
Driver Download
  ↓
Driver Installation
  ↓
Driver Verification
  ↓
Application Detection
  ↓
Application Installation
  ↓
Final Verification
  ↓
Report
  ↓
DONE
```

Pipeline harus menghasilkan structured events:

```cpp
ProvisioningEvent
{
    timestamp
    category
    severity
    message
    progress
}
```

Contoh:

```text
[10:31:02] INFO  Detecting hardware
[10:31:04] INFO  Found 3 devices requiring drivers
[10:31:05] INFO  Searching driver for Realtek PCIe GbE
[10:31:07] INFO  Driver found
[10:31:08] INFO  Downloading driver
[10:31:21] INFO  Installing driver
[10:31:25] SUCCESS Driver installed
```

---

# Error Handling

Satu kegagalan tidak boleh menghentikan seluruh provisioning.

Contoh:

```text
Driver A
✓ Installed

Driver B
✓ Installed

Driver C
✗ Not found
→ Skip

Driver D
✓ Installed

Application A
✓ Installed

Application B
✗ Failed
→ Log error
→ Continue
```

Di akhir tampilkan summary.

---

# Reboot Handling

Beberapa driver mungkin membutuhkan reboot.

Jangan reboot otomatis pada V1 kecuali secara eksplisit diperlukan.

Sebagai gantinya:

```text
Reboot Required: YES
```

Setelah reboot, aplikasi nantinya dapat dilanjutkan dari state sebelumnya.

Untuk V1, minimal desain state machine agar fitur resume memungkinkan di masa depan.

---

# Logging

Gunakan structured logging.

Minimal:

```text
logs/
└── 2026-09-03_103000.json
```

Simpan:

* system information
* Windows version
* architecture
* device detection
* driver search
* download
* installation
* application installation
* errors
* duration
* final result

Jangan menyimpan credential atau data sensitif.

---

# Report

Setelah provisioning selesai, tampilkan:

```text
Provisioning Complete

Hardware
--------
Devices detected: 42
Devices requiring driver: 5

Drivers
-------
Already installed: 37
Installed: 4
Not found: 1
Failed: 0

Applications
------------
Installed: 7
Already installed: 2
Failed: 0

Reboot required: YES

Duration: 17m 32s

Status: SUCCESS WITH WARNINGS
```

Report juga harus dapat disimpan sebagai JSON.

---

# CLI First

Sebelum membuat GUI kompleks, implementasikan CLI.

Contoh:

```text
provisioner.exe scan
provisioner.exe drivers scan
provisioner.exe drivers install
provisioner.exe apps install --profile standard
provisioner.exe provision --profile standard
provisioner.exe report
```

Pastikan seluruh core functionality dapat diuji melalui CLI.

GUI hanya menjadi frontend terhadap core engine.

---

# GUI

Setelah core stabil, buat Qt GUI sederhana.

Dashboard:

```text
┌─────────────────────────────────────┐
│ Amanah Provisioner                  │
├─────────────────────────────────────┤
│                                     │
│ System                              │
│ Windows 11 Pro                      │
│ Intel Core i5                       │
│ RAM 16 GB                           │
│                                     │
│ Drivers                             │
│ ████████████████░░░ 80%             │
│                                     │
│ Applications                       │
│ ███████████░░░░░░░░ 55%             │
│                                     │
│ Current task                        │
│ Installing Realtek Audio Driver     │
│                                     │
│ [ Pause ]       [ Cancel ]          │
└─────────────────────────────────────┘
```

GUI harus mendapatkan data melalui events dari core engine.

Jangan menaruh business logic provisioning di widget/UI class.

---

# Security

Anggap driver sebagai security-sensitive component.

Jangan:

* menjalankan executable driver secara sembarangan
* menonaktifkan Windows Defender
* menonaktifkan driver signature enforcement
* melakukan registry hack untuk bypass security
* menggunakan driver unsigned tanpa alasan
* menjalankan script remote tanpa validasi

Prioritaskan:

```text
Trusted source
→ Download
→ Verify
→ Extract
→ Validate
→ Install
```

Jika package tidak dapat diverifikasi/reliable, report sebagai warning/error dan skip.

---

# Compatibility

Target:

```text
Windows 7 32-bit
Windows 7 x64
Windows 8 x64
Windows 10 x64
Windows 11 x64
```

Jangan support ARM terlebih dahulu.

Deteksi:

```text
Windows version
Build
Architecture
UEFI/BIOS
Administrator privilege
Internet connectivity
WinGet availability
PnPUtil availability
```

Jika requirement tidak terpenuhi, tampilkan error yang jelas.

---

# Testing

Buat unit test untuk:

* Hardware ID parsing
* device classification
* driver matching
* profile parsing
* provisioning state
* error handling

Buat integration test untuk:

* PnPUtil execution
* WinGet detection
* package installation
* driver verification

Jangan melakukan destructive driver testing pada development machine utama.

Gunakan VM/test hardware untuk integration testing.

---

# Development Strategy

Jangan langsung membangun seluruh fitur.

Implementasikan secara bertahap:

### Milestone 1

```text
CLI
↓
Enumerate devices
↓
Display missing/problem drivers
```

### Milestone 2

```text
Hardware ID
↓
Driver Provider abstraction
↓
Mock DriverProvider
```

### Milestone 3

```text
Real DriverPackProvider
↓
Download
↓
Cache
```

### Milestone 4

```text
Install INF
↓
PnPUtil
↓
Verify
```

### Milestone 5

```text
WinGet
↓
Application Profiles
↓
Install
```

### Milestone 6

```text
ProvisioningEngine
↓
End-to-end provisioning
```

### Milestone 7

```text
Qt GUI
↓
Progress
↓
Logs
↓
Report
```

---

# Important Agent Rules

Sebelum menulis banyak kode:

1. Inspect repository terlebih dahulu.
2. Tentukan build system.
3. Tentukan Qt version.
4. Buat architecture/design document singkat.
5. Identifikasi API Windows yang akan digunakan.
6. Jangan membuat asumsi tentang DriverPack.
7. Investigasi mekanisme DriverPack sebelum mengimplementasikan provider.
8. Jangan membuat scraper yang rapuh tanpa alasan.
9. Jangan membuat GUI sebelum core engine memiliki API yang jelas.
10. Jangan over-engineer dengan microservices atau backend server.
11. Untuk V1, aplikasi harus dapat berjalan standalone di laptop teknisi.
12. Semua operasi eksternal harus memiliki timeout dan error handling.
13. Jangan menghentikan seluruh provisioning hanya karena satu driver/application gagal.
14. Semua hasil harus dapat ditelusuri melalui log.
15. Setelah setiap milestone, build dan test aplikasi sebelum melanjutkan.

## First Task

Jangan langsung mengimplementasikan semua fitur.

Mulai dengan:

1. Inspect repository.
2. Buat architecture proposal.
3. Buat project skeleton.
4. Implementasikan `DeviceEnumerator`.
5. Implementasikan CLI:

```text
provisioner.exe scan
```

yang menampilkan seluruh device dan secara khusus menandai device yang membutuhkan driver.

Setelah itu build dan test.

Berikan ringkasan perubahan yang dibuat dan hasil test sebelum melanjutkan ke milestone berikutnya.
