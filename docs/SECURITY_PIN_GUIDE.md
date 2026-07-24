# KernSigner PIN and Security Operations Guide

Date: 2026-05-21

This document is for anyone using KernSigner for the first time. The goal is simple: when you see a PIN input box on screen, know which PIN to enter, what happens if you get it wrong, and when you should stop.

**Current firmware is still in test fund acceptance mode**, not an audited commercial production wallet. Do not import real mnemonic phrases or store real assets until you have completed the commercial release gate, device acceptance testing, and independent security audit; use only test mnemonics, testnet assets, or small acceptance funds you can explicitly afford to lose.

For complete smart card operation flows, see [Satochip / SeedKeeper Smart Card Tested Operations Guide](SMARTCARD_SATOCHIP_SEEDKEEPER_OPERATION_GUIDE.md).

## Read This First — 30 Seconds Overview

There are two types of PINs in KernSigner that must NOT be mixed.

| Name | Protects Whom | When to Enter | Wrong Input Consequence |
|---|---|---|---|
| Device PIN | KernSigner development board, local mnemonics, local signing, backup entries | Boot unlock, entering local sensitive features, changing device PIN | Counts toward device wrong attempts. 3 consecutive wrong clears current session and shuts down or reboots |
| Smart Card PIN | Satochip/SeedKeeper smart card plugged into card reader | Page says `Smart Card PIN`, Satochip signing, reading card addresses/public keys, card maintenance | Counts toward smart card's own wrong attempts. Remaining attempts reaching zero locks the card |
| Smart Card PUK | Rescue code for smart card PIN | Page says `PUK` or `Unlock` | Counts toward smart card PUK wrong attempts. PUK running out is very troublesome; may require resetting the card or finding official card tools |

Simplest judgment method:

| Screen Text | What to Enter |
|---|---|
| `Enter PIN` | Device PIN |
| `Set PIN` | New device PIN |
| `PIN Settings` / `Change PIN` | First enter current device PIN, then new device PIN |
| `Smart Card PIN` | Smart card PIN, NOT device PIN |
| `Old PIN` / `New PIN` / `PIN Number`, located on Satochip or SeedKeeper maintenance page | Smart card old PIN, new PIN; ordinary users fill `PIN Number` as `0` |
| `PUK` / `Unlock` | Smart card PUK, NOT regular PIN |

If unsure which PIN it is, do NOT guess. Go back to the previous page first and check the title and current menu.

## What Is Device PIN?

Device PIN is KernSigner device's own PIN. It has nothing to do with smart cards.

Device PIN protects:

| Feature | Description |
|---|---|
| Boot unlock | Device must be unlocked after startup |
| Local mnemonics | Protects mnemonics and sessions already loaded into KernSigner |
| Local backups | Viewing mnemonics, QR codes, entropy, steel plate/dot matrix backups requires device PIN first |
| Local addresses/public keys | Viewing addresses, extended public keys and other sensitive derivation info requires device PIN first |
| Local BTC signing | PSBT or message signing requires device PIN, with final confirmation |
| Changing device PIN | Entering `Settings -> PIN` verifies current device PIN first |

Device PIN length:

| Item | Current Rule |
|---|---|
| Minimum | 6 characters |
| Maximum | 16 characters |
| Characters | Can use numbers, letters, and symbols |

Recommend using a PIN that cannot be easily guessed; do NOT use birthdays, phone numbers, `123456`, `111111`.

## Setting Device PIN for the First Time

When starting the device for the first time, if no device PIN is set yet, the device will automatically enter the Set PIN page.

Operation steps:

1. See `Set PIN`.
2. Enter your new device PIN.
3. Press the confirm key on keyboard.
4. If firmware prompts `Processing...`, wait for completion.
5. After setup completes, it enters the main interface.

Notes:

| Situation | How to Handle |
|---|---|
| Prompts `PIN must be at least 6 characters` | Re-enter at least 6 characters |
| Does not enter main interface after input | Wait for processing; do NOT tap continuously |
| Forgot the PIN you just set | Cannot be recovered from device; can only clean device and restore wallet using mnemonic backup |

Current build's device PIN setup flow saves new PIN directly after input. Future versions may show a `Confirm PIN` page, in which case enter the same PIN again as prompted on screen.

## Changing Device PIN

Path:

```text
Home -> Settings -> PIN
```

Operation steps:

1. Enter `Settings`.
2. Select `PIN`.
3. If PIN has been set before, you first see `Enter PIN` — enter current device PIN here.
4. Enter `PIN Settings`.
5. Select `Change PIN`.
6. When you see `Set PIN`, enter new device PIN.
7. Wait for save to complete.

Do NOT enter smart card PIN here. This changes the device PIN, and does NOT change Satochip or SeedKeeper card PINs.

## What Happens If Device PIN Is Wrong?

Current rule: 3 consecutive wrong attempts triggers protection.

| Consecutive Wrong Attempts | Screen/Device Behavior | Consequence |
|---|---|---|
| 1st time | Shows PIN error, brief wait | Still have chances |
| 2nd time | Shows PIN error, waits longer | One last chance remaining |
| 3rd time | Shows consecutive error prompt, clears current session, attempts shutdown or reboot | Currently loaded wallet is cleared from RAM; need to re-unlock or reload |

Important notes:

| Question | Answer |
|---|---|
| Does 3rd wrong clear all device storage? | Current build does NOT treat this as "factory reset"; it clears current session and shuts down/reboots |
| What happens to error count after correct input? | Correct input resets the device PIN error count to zero |
| Does error count auto-reset after shutdown? | Do NOT rely on shutdown to reset. When you see errors approaching 3, stop trying; confirm PIN before entering |
| Forgot device PIN? | Cannot be recovered. Can only clean device and restore wallet using mnemonics or backups |

## Auto-Lock and Auto-Shutdown

Current security policy is fixed:

| Item | Current Value | Behavior |
|---|---|---|
| Auto-lock | 3 minutes inactivity | Clears current sensitive session, returns to device PIN unlock |
| Auto-shutdown protection | 10 minutes inactivity | Clears current sensitive session and attempts shutdown; if hardware cannot software-shutdown, reboots instead |

After auto-lock, you enter the device PIN.

## Where Do You Need to Enter Device PIN?

When these features pop up `Enter PIN`, enter your device PIN.

| Scenario | Which PIN | Description |
|---|---|---|
| Device boot | Device PIN | Unlock KernSigner |
| Re-use after auto-lock | Device PIN | Re-open session |
| `Home -> Settings -> PIN` | Device PIN | Verify current PIN before modifying settings |
| Backup mnemonics, backup QR codes, backup entropy, steel plate/dot matrix backups | Device PIN | Followed by sensitive data danger confirmation |
| View local extended public key | Device PIN | Protects local wallet info |
| View local addresses | Device PIN | Protects local wallet info |
| Manage local mnemonic slots | Device PIN | Protects local wallet info |
| BTC PSBT signing | Device PIN | After PIN, must re-verify transaction and confirm |
| BTC message signing | Device PIN | After PIN, must re-verify message and confirm |

When Web3 QR signing, if you select `Mnemonic` as the signing source, the page will NOT ask you to enter smart card PIN; if page shows `Enter PIN`, enter device PIN; if it does not appear, continue after verifying info on confirmation page. Only enter smart card PIN when the page says `Smart Card PIN`.

## What Is Smart Card PIN?

Smart card PIN is Satochip or SeedKeeper card's own PIN. It exists on the card; KernSigner development board does NOT know it and cannot recover it for you.

Smart card PIN protects:

| Feature | Description |
|---|---|
| Satochip Web3 signing | Card confirms PIN before signing |
| Generating Web3 connection codes | Reading card account info requires PIN |
| Reading Satochip addresses by path | Reading card path addresses requires PIN |
| Reading BTC watch public keys | Reading card xpub/ypub/zpub etc. requires PIN |
| Satochip/SeedKeeper maintenance | Changing card PIN, changing tags, policies, reset and other high-risk operations require card PIN; SeedKeeper primarily stores secrets, NOT equivalent to signing cards |

Device PIN and smart card PIN do not affect each other:

| Action | Who It Affects |
|---|---|
| Device PIN wrong | Only affects KernSigner development board |
| Smart card PIN wrong | Only affects the smart card |
| Changing device PIN | Does NOT change smart card PIN |
| Changing smart card PIN | Does NOT change device PIN |

## When to Enter Smart Card PIN?

When page title or input box says `Smart Card PIN`, enter your smart card PIN.

Common paths:

| Path | Which PIN | Purpose |
|---|---|---|
| `Connect Wallet -> Web3 -> Smart Card` | Smart Card PIN | Generate Satochip Web3 connection QR code |
| `QR Sign -> Web3 Request -> Smart Card` | Smart Card PIN | Sign Web3 request using Satochip |
| `Smart Card Tools -> Satochip -> Address` | Smart Card PIN | Read addresses by path |
| `Smart Card Tools -> Satochip -> BTC Public Key` | Smart Card PIN | Read xpub/ypub/zpub/tpub/upub/vpub |
| `Smart Card Tools -> Satochip -> Maintenance` | Smart Card PIN or PUK | High-risk maintenance, see below |
| `Smart Card Tools -> SeedKeeper` pages requiring card authorization | Smart Card PIN | SeedKeeper card operations |

Current screen main menu typically shows as `Home -> Smart Card`; old documents or old firmware may say `Smart Card Tools`, referring to the same class of smart card entries.

If just `Smart Card Tools -> Satochip -> Info` or card reader detection, it's generally reading status; do NOT randomly enter PINs. First check whether the page explicitly says `Smart Card PIN`.

## What Happens If Smart Card PIN Is Wrong?

Smart cards have their own remaining attempts. KernSigner reads card return codes and displays something like:

```text
PIN error, 2 attempts remaining
```

Or on status pages:

```text
PIN Remaining: PIN0=3 PUK0=5 PIN1=3 PUK1=5
```

Specific remaining attempts depend on your actual card display.

| Situation | Consequence |
|---|---|
| Smart card PIN wrong but still has remaining attempts | This card's PIN remaining attempts decrease |
| Smart card PIN remaining attempts become 0 | Card PIN locks; KernSigner shows `PIN Locked` or similar error |
| Continue signing after PIN locked | Will not succeed; must use PUK to unlock first |
| Wrong smart card PIN | Does NOT increase device PIN error count |
| Wrong device PIN | Does NOT increase smart card PIN error count |

If remaining attempts are only 1, do NOT continue guessing. Confirm card PIN records first, or switch to official tools/maintenance flows.

## Changing Smart Card PIN

This is a high-risk operation. Only operate when you confirm you know the current smart card PIN AND have enough remaining card attempts.

Path:

```text
Smart Card Tools -> Satochip -> Maintenance -> Change PIN
```

Operation steps:

1. Plug in card reader and Satochip card.
2. Enter `Smart Card Tools`.
3. Enter `Satochip`.
4. Enter `Maintenance`.
5. Select `Change PIN`.
6. In `Old PIN`, enter current smart card PIN.
7. In `New PIN`, enter new smart card PIN.
8. Ordinary users fill `PIN Number` as `0`.
9. Press `Execute`.
10. Result showing `PIN Modified` means success.

Do NOT enter device PIN in `Old PIN`. This changes the card PIN.

SeedKeeper card's Change PIN entry is similar:

```text
Smart Card Tools -> SeedKeeper -> Change PIN
```

Also enters SeedKeeper card PIN, NOT device PIN.

Newly flashed or just-reset SeedKeepers must set PIN first:

```text
Smart Card Tools -> SeedKeeper -> Set PIN
```

Factory restore entry is called `Reset`:

```text
Smart Card Tools -> SeedKeeper -> Reset
```

**`Reset` will clear ALL secrets on the card and return to uninitialized state. Only proceed if you confirm you have paper or other offline backups AND this is indeed the test card to be cleared.**

New version SeedKeeper's `Reset` is NOT old `B0 FF` plug/unplug flow. The correct method is: deliberately enter wrong PIN until PIN locked, then deliberately enter wrong PUK until returning `FF00`. The menu's `Wrong PIN Step` and `Wrong PUK Step` are for this exact flow. Do NOT enter real PIN or real PUK on the reset page.

## Smart Card PUK and Unlock

PUK is the rescue code after smart card PIN locks; it is NOT a regular PIN.

Path:

```text
Smart Card Tools -> Satochip -> Maintenance -> Unlock
```

Current unlock flow is for PIN0:

1. Enter `Unlock`.
2. Page shows `PUK`.
3. Enter smart card PUK.
4. Press `Execute`.
5. Result showing `PIN Unlocked` means success.

Risk notes:

| Situation | Risk |
|---|---|
| Wrong PUK | Smart card PUK remaining attempts decrease |
| PUK remaining attempts reach zero | May not be able to unlock normally; may need official tools, reset card, or rely on mnemonic backup recovery |
| Don't know PUK? | Do NOT guess; stop first |

## How to Choose PIN for Web3 Signing

After scanning a Web3 request, first check the signing source.

| Source You Selected | What to Enter Next |
|---|---|
| `Mnemonic` | Do NOT enter smart card PIN; verify confirmation page info |
| `Smart Card` | Verify confirmation page first, then enter smart card PIN on `Smart Card PIN` page |

Before signing, you MUST verify:

| Field | What to Check |
|---|---|
| Source | Is it `Mnemonic` or `Smart Card`? |
| Wallet/DApp | Is it the wallet and webpage you're actively operating? |
| Chain | Is it the target chain? |
| Path | Is it the account path you want to use? |
| Address | Is it the address you want to sign? |
| Recipient Address | Is it correct? |
| Amount | Is it correct? |
| Gas / Fee | Is it acceptable? |
| Transaction Data | If incomprehensible or abnormal, do NOT sign |

TypedData/EIP-712 cannot be signed currently. The page allows verification but will prompt that EIP-712 hash is not yet connected and stop signing.

## How to Choose PIN for BTC Signing

BTC PSBT or BTC message signing uses KernSigner's local mnemonic signing flow.

| Scenario | Which PIN |
|---|---|
| PSBT signing | Device PIN |
| BTC message signing | Device PIN |
| Satochip BTC watch public key | Smart Card PIN |

Do NOT treat `Satochip BTC Watch Public Key` as `Satochip BTC Card Signing`. In the current main line, BTC card signing is NOT the same flow.

## Where Beginners Make Mistakes Most Often

| Common Mistake | Correct Approach |
|---|---|
| Seeing PIN and entering the same PIN every time | Check title first: `Enter PIN` = device, `Smart Card PIN` = card |
| Using device PIN to sign Satochip | Wrong. Satochip signing requires smart card PIN |
| Using smart card PIN to unlock device | Wrong. Boot unlock requires device PIN |
| Still guessing when card PIN has 1 attempt left | Stop; do NOT lock the card |
| Using PUK as a regular PIN | Wrong. PUK is ONLY for unlocking card PIN |
| Randomly filling `PIN Number` when changing smart card PIN | Ordinary users fill `0` |
| Tapping sign without reading confirmation page | Must verify address, amount, chain, path, and DApp |

## How to Diagnose When Problems Occur

| Symptom | Possible Cause | Handling |
|---|---|---|
| Cannot enter main interface after boot | Device PIN wrong or still waiting for countdown | Wait for countdown to end; confirm device PIN before trying again |
| Shows `PIN consecutively wrong 3 times` | Device PIN wrong 3 consecutive times | Wait for device shutdown/reboot; confirm PIN; do NOT keep guessing |
| Web3 smart card signing fails, shows remaining attempts | Smart card PIN wrong | Stop guessing; confirm card PIN |
| Shows `PIN Locked` | Smart card PIN attempts exhausted | Need PUK to unlock |
| Shows `Please enter Smart Card PIN first` | Page needs card PIN | Enter smart card PIN, NOT device PIN |
| Shows `Please create PIN code in Settings first` | Device PIN not yet set | Go to `Settings -> PIN` or follow guide after reboot |
| Don't know which PIN to enter now | Page title unclear or unsure yourself | Go back one page; re-judge from menu path |

## Security Habits

| Recommendation | Reason |
|---|---|
| Do NOT set device PIN and smart card PIN the same | If one leaks, it should not affect the other |
| Do NOT write PINs on device back or card sleeve | Losing device/card means simultaneous compromise |
| Do NOT keep trying when card PIN remaining attempts are low | Card lock recovery cost is high |
| Keep mnemonic backups safe | Forgetting PIN, lost cards, broken cards can only be recovered via backups |
| Verify transactions item-by-item before signing | PIN only proves you authorized; it does NOT judge whether the transaction is safe |
| Do NOT tap unfamiliar maintenance items | Changing tags, policies, reset, factory restore etc. may all affect card status |

## One-Sentence Summary

See `Enter PIN`? Enter device PIN. See `Smart Card PIN`? Enter smart card PIN. See `PUK`? Only enter PUK when unlocking a smart card. When any remaining attempts approach 0, do NOT keep guessing.