[简体中文](ai-cockpit-0.5.1.zh_CN.md)

# AI Cockpit 0.5.1

The community update incorporates the personal 0.4.8 UI and synchronization work
while retaining user-provided NVS API settings. No personal endpoint or key is
compiled into this build. Configure your API before Bluetooth Wi-Fi setup as
shown in README; writing the settings partition clears its previous contents.

## Controls and changes

Hold OK to enter or leave SETTINGS; arrows select WI-FI, SYNC, BLE, LIGHT or
SLEEP, and short OK activates the row. LIGHT cycles 10/20/35/50/70/100 percent,
with a 35-percent default. SLEEP cycles 15/30/60/120/300 seconds and ALWAYS ON;
the default is 30 seconds. Values persist. Startup leaves the screen off;
ALWAYS ON disables idle blanking only after waking. Sync continues while blank.
The softer neon palette, darker background and shaded digits replace 0.5.0's
flat digits. Information-page navigation remains unchanged.

Sync keeps board data separate from status updates, merges presses during a
request, tries current DNS alternatives and supports short-lived HTTPS reuse.
TLS certificate and hostname checks remain enabled. Connection and response
phase timeouts shorten waiting, but blocking transport operations may exceed
the configured timeout. This is not a guarantee of instant or error-free sync.
Stale backend data remains CACHED. The backend retries upstream failures after
60 seconds, retaining a 300-second successful cadence and private error logs.

## Validation scope

The personal 0.4.8 implementation was device-tested for startup, fresh HTTPS
responses, repeated-button merging, the five settings rows and 15-second idle
blanking. The user confirmed SYNCED after automatic address recovery. Intermittent
TLS timeouts remain; connection reuse has not been confirmed on device.
This public 0.5.1 build retains NVS provisioning and requires its own complete
host/build/archive validation; it is not claimed to have been flashed to a device.
Release images remain inspected renders with synthetic data from the prior version.

## Build result

Build: PASS. Host tests: PASS. Archive verification: PASS.
Device tests: NOT RUN for this community artifact.

Full SHA-256: `fcd1bff693d7ee5d6fe8eb88ca1cce8cfe8d06e7974d77b8fcefdb354e6aa61d`

ELF SHA-256: `4bfacd8cae84597c08e7426acda74a5574344c4fa2f8130d6bcf088f41fd3dfb`
