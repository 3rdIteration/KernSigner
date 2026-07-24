# Mnemonic Backup and Recovery Guide

Date: 2026-05-21

This document explains what scenario each backup method in KernSigner is suitable for, how to do it, how to verify, and how to recover. Beginners just need to remember one thing: after creating a wallet, backup first and practice recovery before putting ANY assets into it.

**Current firmware is still in test fund acceptance mode.** Do not import real mnemonic phrases or store real assets until you have completed the commercial release gate, device acceptance testing, and independent security audit; use only test mnemonics, testnet assets, or small acceptance funds you can explicitly afford to lose.

## Security Rules Before Backup

| Rule | Reason |
| --- | --- |
| No photos, no screenshots, no cloud uploads | Phones and cloud are the easiest to leak |
| Do NOT enter real mnemonics into internet-connected web pages | Web pages, extensions, input methods may all leak |
| Do NOT rely on only one backup type | Paper can be lost, metal can be engraved wrong, files can be corrupted |
| Immediately practice recovery after backup | Unpracticed backups are uncertain |
| Start with small test amounts first | Confirm addresses and signing flows are correct |

## Backup Entry

Common path:

```text
Home -> Mnemonic -> Current Mnemonic -> Backup Export
```

If the page asks for `Enter PIN`, enter your device PIN. This is NOT smart card PIN.

## How to Choose Backup Methods

| Method | Advantages | Risks | Recommended Use |
| --- | --- | --- | --- |
| Plain text mnemonic | Most universal, any wallet can recover | If seen, can be stolen | Must-do basic backup |
| Mnemonic QR code | Fast recovery, less typing | QR codes = mnemonic phrase | Only use temporarily in secure environments |
| Raw entropy | Can precisely verify with BIP39 tools | Advanced material; leaks equally dangerous | Verification and advanced backups |
| Steel plate / 1248 / Dot matrix | Fire/water resistant, suitable for long-term storage | Errors hard to discover | Long-term cold backup |
| Tinyseed / Stackbit | Suitable for corresponding physical boards | Need to restore by format | Dedicated physical backups |
| KEF encrypted backup | Can store to card or file | Password loss = unrecoverable | Extra copy; does NOT replace paper backup |
| Smart card write / SeedKeeper | Save or generate on card | Complex PIN/PUK management | Use after familiarizing with smart cards |

## Plain Text Mnemonic Backup

Operation:

1. Enter `Backup Export`.
2. Select plain text mnemonic or index page.
3. Write down all words in order on paper.
4. Do NOT only write words without writing indices.
5. After writing, verify each word one by one.
6. Store the paper where it cannot be photographed or seen by others.

Verification:

1. Re-open the backup page.
2. Verify each entry against what's on paper.
3. Especially check similar-looking words, missing words, and order errors.

Recovery drill:

1. Clear current session, or use a different test device.
2. Select `Import -> Manual`.
3. Enter in the order from your paper.
4. Verify wallet fingerprint and addresses match.

## Mnemonic QR Code Backup

QR codes contain the mnemonic itself; risks are equally high as plain text mnemonics.

Suitable for:

- Temporary migration between your own devices.
- Recovery drills in secure offline environments.

NOT suitable for:

- Printing and storing randomly.
- Taking photos with phone to save.
- Sending to others or uploading to cloud.

Verification:

1. Use KernSigner's QR scan import function to scan back.
2. Or parse with offline tools.
3. Verify fingerprint and addresses.

## Raw Entropy Backup

Raw entropy is the core random data before BIP39 generates mnemonics. Its leak is equivalent to mnemonic leak.

Path:

```text
Home -> Mnemonic -> Current Mnemonic -> Raw Entropy
```

The page displays:

| Field | Description |
| --- | --- |
| Fingerprint | Current wallet fingerprint |
| Entropy bits | 128/160/192/224/256 bits |
| Raw input | If created via dice, coin, playing cards etc., shows input materials |
| Hex | BIP39 raw entropy |

Verification methods see [MNEMONIC_CREATION_BIP39_VERIFY.md](MNEMONIC_CREATION_BIP39_VERIFY.zh-CN.md).

Recommendations:

- You can copy hex as an advanced backup.
- Group every 4 or 8 characters when copying.
- Remove spaces when recovering.

## Metal Backups

Metal backups are suitable for long-term storage, but the biggest fear is engraving errors.

### Steel Plate / 1248 / Dot Matrix

Before operation:

1. Confirm whether you're using a test mnemonic or formal mnemonic.
2. Clearly read page title and format.
3. Prepare physical board instructions.
4. Read back each line after engraving.

Recovery drill:

1. Use the corresponding restore method in `Import`.
2. Enter according to physical board content.
3. After recovery, verify fingerprint and first few addresses.

Acceptance focus:

- Correct word count.
- Indices or dot positions not shifted by one.
- Recovered fingerprint matches.
- Same derivation path addresses match.

## KEF Encrypted Backup

KEF is encrypted backup. It's suitable as an "extra copy"; NOT recommended as the only backup.

Operation recommendations:

1. Use a strong password.
2. Password should NOT be the same as device PIN.
3. Store KEF file and password in separate locations.
4. Immediately test import after backup.

Risks:

| Situation | Consequence |
| --- | --- |
| KEF file lost | Cannot recover from file |
| Password lost | File cannot be decrypted |
| File and password both leaked | Mnemonic may be compromised |

## Smart Card Backups and SeedKeeper

Smart card features involve card PIN, PUK, remaining attempts, and export permissions. Beginners should NOT use them as the only backup.

Recommended flow:

1. First backup mnemonic on paper.
2. Then test smart card read status.
3. Confirm smart card PIN/PUK records are correct.
4. Then consider writing to or loading from SeedKeeper.
5. Record card label, SID, and fingerprint after each smart card operation.

Smart card PIN details see [SECURITY_PIN_GUIDE.md](SECURITY_PIN_GUIDE.zh-CN.md).

## What Must Be Verified After Recovery

After successful recovery, do NOT immediately transfer funds; first verify:

| Item | How to Check |
| --- | --- |
| Wallet fingerprint | KernSigner home page or mnemonic menu |
| 1st receiving address | Compare KernSigner address page with watch wallet |
| Extended public key | Compare KernSigner public key page with watch wallet |
| Network | Mainnet/testnet must match |
| Derivation path | `m/84'/0'/0'`, `m/84'/1'/0'` etc. must match |
| BIP39 passphrase | With or without passphrase produces completely different wallets |

If fingerprint or addresses do NOT match, STOP; do NOT transfer funds.

## Recommended Backup Combinations

Test funds:

1. Plain text paper backup.
2. Offline BIP39 verification.
3. One recovery drill.

Minimum combination before long-term formal use:

1. One paper backup.
2. One metal backup.
3. Optional: one KEF encrypted file.
4. Store in two different locations.
5. Small test receiving and signing.
6. Complete commercial release gate before considering formal use.

## Common Issues

### Is QR code backup alone sufficient?

Not recommended. QR codes can be damaged, format incompatible, or photographed. Paper word backups remain the most universal.

### Which is better: raw entropy backup or mnemonic backup?

Beginners should backup mnemonics. Raw entropy is suitable for verification and advanced backups. Both have equally high leak risks.

### What if I forget my passphrase?

Cannot derive passphrase from mnemonic. If you use a passphrase, you must record it separately.

### What if recovered addresses don't match?

Check in order: are words the same? Is order the same? Is passphrase the same? Is network the same? Is derivation path the same?

### What if smart card breaks?

If you have paper mnemonic backup, you can restore to a new device. If you only have the smart card, recovery depends on card export permissions, PIN/PUK, and card status; so do NOT rely solely on smart cards.