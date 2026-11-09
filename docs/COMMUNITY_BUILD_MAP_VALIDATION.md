# Community executable map validation

`hook/sites_v12_4.inc`, `sites_v12_5.inc`, and `sites_v12_6.inc` translate v1.0 guest addresses to the community
executables. Rows with a nonzero third field fingerprint four bytes at the translated address; zero means the row
only records an address and has no byte fingerprint. Check these tables against legally obtained local executables
with:

```powershell
python tools/check_port_site_tables.py `
  --case v1.2.4-beta path/to/race.bin hook/sites_v12_4.inc `
  --case v1.2.5-2016 path/to/race.bin hook/sites_v12_5.inc `
  --case v1.2.6-2017 path/to/race.bin hook/sites_v12_6.inc `
  --json out/port-site-map-validation.json
```

The current local validation found all 9,406, 9,403, and 9,399 mapped addresses inside PE sections for v1.2.4 beta,
v1.2.5 2016, and v1.2.6 2017, respectively. It also checked 3,176 table fingerprints and all four full adapter
prologues per build; all matched the corresponding executable bytes. Of the mapped rows, 6,230, 6,227, and 6,223
are address-only rows without a byte fingerprint. The output report is ignored under `out/`.

This proves that mapped addresses lie within PE sections and that the recorded byte fingerprints agree with these
three local executable images. It does not verify the semantic correctness of address-only mappings, hook installation
or dialog ABI at runtime, exact archive loading, or race playability.
The latest v1.2.6 patch remains the primary compatibility target. Asset-backed integration testing for v1.1 and v1.2.4
through v1.2.5 is still outstanding.
