# Milestone 3 — DriverPack investigation + Downloader + Cache

**Goal:** (a) complete the DriverPack investigation and record the decision; (b) a robust
`DriverDownloader` with a deterministic cache. A real network provider is implemented
**only if** the investigation yields a reliable + permitted mechanism.

Prereq: Milestone 2 accepted. Read [../DRIVER_PROVIDER.md](../DRIVER_PROVIDER.md) fully.

---

## Task 1 — DriverPack / driver-source investigation (NO provider code yet)

Work the checklist in [../DRIVER_PROVIDER.md](../DRIVER_PROVIDER.md) "Investigation
checklist". For each item, capture concrete evidence (a real request/response, a quoted
ToS clause, a screenshot/console log). Specifically decide:

- [ ] Official API? (base URL, auth, schema) — yes/no + evidence.
- [ ] Direct stable download by Hardware ID possible? — captured example or "no".
- [ ] Licensing/ToS permit automated download + redistribution to technician machines? —
      quoted clause. **If unclear or prohibited: stop, escalate, do not scrape.**
- [ ] Integrity mechanism (checksums/signatures)? — yes/no.
- [ ] Alternatives compared: Windows Update driver catalog (`IUpdateSearcher` with the
      driver category / `IWindowsUpdateAgentInfo`), Microsoft Update Catalog, per-vendor
      catalogs, an internal driver mirror/share. Pros/cons table.

**Deliverable:** write **ADR-0004** in [../DECISIONS.md](../DECISIONS.md) with the decision:
one of
  1. implement `DriverPackProvider` against the documented reliable+permitted mechanism,
  2. implement `WindowsUpdateProvider` (COM `IUpdateSearcher`, driver category) as the real
     path,
  3. implement `MirrorProvider` (internal HTTP/file share, curated index) as the real path,
  4. ship with `MockDriverProvider` only + documented integration point.

Get human sign-off on ADR-0004 before Task 3.

## Task 2 — DriverDownloader + cache (independent of Task 1's outcome)

- [ ] `src/core/drivers/DriverDownloader.h/.cpp` using `QNetworkAccessManager`:
      - `struct DownloadRequest { DriverPackage package; }` /
        `struct DownloadResult { bool ok; std::string localPath; std::string error;
        bool fromCache; }`.
      - progress callback (`std::function<void(qint64 received, qint64 total)>`),
      - retry: 3 attempts, backoff 1s/4s/9s, per-attempt timeout (default 120s, configurable),
      - resume via HTTP `Range` when server sends `Accept-Ranges: bytes`,
      - download to `<file>.part`, verify, atomic rename.
- [ ] Cache: `packageId = sha256(provider|driverName|version|arch|downloadUrl)[:16]`
      (`QCryptographicHash`). Layout
      `cache/drivers/<packageId>/package.<ext>` + `metadata.json` (the `DriverPackage` +
      checksum + fetch timestamp). Reuse when metadata present and (checksum matches, or no
      checksum and size > 0).
- [ ] Checksum verification when `package.checksum` is set (algo from `checksumAlgo`).
- [ ] `file://` URLs supported (so the Milestone 2 fixtures work as a local "server").

## Task 3 — Real provider (only per ADR-0004 decision)

- [ ] Implement whichever provider ADR-0004 selected. If option 4 (mock only): skip; just
      make sure the integration point (factory slot, config) is clean and documented.
- [ ] `--provider <name>` selects it; document required config/network in
      [../DRIVER_PROVIDER.md](../DRIVER_PROVIDER.md).

## Task 4 — CLI

- [ ] Extend `drivers scan` (or add `drivers resolve`) to also **download** matched packages
      into the cache when `--download` is passed; print cache hits vs fetches; never abort
      the batch on one failure.

## Task 5 — Tests

- [ ] `tests/unit/test_cacheid.cpp`: `packageId` deterministic; changes with version/url.
- [ ] `tests/unit/test_downloader.cpp`: against a **local QTcpServer/QHttpServer fixture**
      (no external hosts) — success, 404, mid-stream disconnect + retry, resume, checksum
      mismatch → failure, cache hit skips network.
- [ ] `scripts\test.bat` green. No test hits the internet.

---

## Exit criteria

- [ ] ADR-0004 written, evidence-backed, human-signed-off.
- [ ] `DriverDownloader` unit tests pass against a local fixture server (offline).
- [ ] Cache dedupe demonstrated (second resolve of the same package = "from cache").
- [ ] If a real provider was built: one real end-to-end resolve+download works (on a
      network-connected run) and is shown; failures are per-package, non-fatal.
- [ ] Change summary + ADR + test output. STOP for review.
