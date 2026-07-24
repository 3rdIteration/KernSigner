# KernSigner User Quick Start Guide

Date: 2026-05-21

This document is for anyone who receives a KernSigner development board for the first time. Follow these steps to learn how to power on, set up a PIN, create or import a test mnemonic phrase, backup, receive funds, sign transactions, and safely exit.

**Current firmware is a test fund acceptance version.** Do not import real mnemonic phrases or store real assets until you have completed the commercial release gate, device acceptance testing, and independent security audit. Use only test mnemonic phrases, testnet assets, or small amounts of acceptance funds you can afford to lose.

## What You Need

| Item | Purpose |
| --- | --- |
| KernSigner development board | Offline creation/import of mnemonics, display addresses, QR code signing |
| USB data cable | Power, flashing firmware, serial log capture |
| Camera module | Scan QR codes, camera-based mnemonic random generation; manual focus adjustment required on first use |
| Paper and pen | Write down mnemonics and backup info; record mnemonics and PIN/PUK separately and store them in different locations |
| Mobile or desktop wallet app | Import watch-only wallet, initiate PSBT or Web3 signing |
| Smart card and reader | Only needed when using Satochip/SeedKeeper |

## First Boot

1. Plug in USB power.
2. Wait for the screen to enter the PIN prompt or home page.
3. If prompted to `Set PIN`, set up your device PIN.
4. If prompted to `Enter PIN`, enter your device PIN.
5. Once on the home page, do not rush to connect a real wallet. Use the test flow first to familiarize yourself with operations.

**Device PIN and Smart Card PIN are different.** Only enter the card PIN when you see `Smart Card PIN`.

When using ESP32-P4 with 4.3" camera for the first time, adjust focus before scanning: open the camera or QR scanner page, hold a standard black-and-white QR code at 10-20 cm distance, and gently rotate the outer ring of the OV5647 lens until the QR code edges appear sharpest. Once standard QR codes scan reliably, test high-density dynamic codes from OKX, Bitget, TokenPocket, etc.

## Creating a Test Mnemonic Phrase

We recommend starting with a 12-word test mnemonic phrase.

Path:

```text
Home -> Mnemonic -> Create
```

Available methods:

| Method | Suitable For | Description |
| --- | --- | --- |
| `Hex` / `十六进制` | Want to fully cross-reference with BIP39 websites | Easiest to verify |
| Coin Flip / `抛硬币` | Want physical randomness | Enter 0/1 |
| Dice / `骰子` | Have a six-sided die | Enter 1-6 |
| D20 | Have a twenty-sided die | Enter 1-20 |
| Playing Cards / `扑克牌` | Have physical playing cards | Enter card faces like `AH QS 9D TC` |
| Camera / `相机` | Want quick random entropy generation | Take photos of complex images |
| Manual / `手动` | Already know the words to enter | Must pass BIP39 validation |
| Smart Card / `智能卡` | Using SeedKeeper/Satochip test card | Randomness from card; SeedKeeper stores secrets, Satochip signs |

For detailed steps, see [MNEMONIC_CREATION_BIP39_VERIFY.md](MNEMONIC_CREATION_BIP39_VERIFY.md).

## Importing an Existing Mnemonic Phrase

Path:

```text
Home -> Mnemonic -> Import
```

Common import methods:

| Method | Purpose |
| --- | --- |
| Manual Entry | Enter 12/15/18/21/24 BIP39 English words |
| QR Scan | Scan SeedQR or supported mnemonic QR codes |
| Number/Index | Enter BIP39 word numbers |
| Steel Plate/Dot Matrix/Tinyseed/Stackbit | Restore from physical backup |
| Smart Card | Load from SeedKeeper/Satochip |

After importing, always verify the fingerprint, addresses, and extended public key. Do not transfer funds just because it says "Import Successful."

## Backing Up Your Mnemonic Phrase

After creating or importing successfully, backup first, then receive funds.

Typical path:

```text
Home -> Mnemonic -> Current Mnemonic -> Backup Export
```

Common backup methods:

| Backup Type | Suitable Scenario |
| --- | --- |
| Plain text mnemonic | Most basic, must write down clearly |
| Mnemonic QR code | Convenient for recovery, but more sensitive |
| Raw entropy | For BIP39 verification and advanced recovery |
| Steel plate/1248/Dot matrix | Long-term physical backup |
| KEF encrypted backup | Encrypted file backup on storage card |

For detailed steps, see [BACKUP_AND_RECOVERY_GUIDE.md](BACKUP_AND_RECOVERY_GUIDE.md).

## Verifying Addresses Before Receiving Funds

Never rely solely on the address shown in your mobile wallet. Always verify on the KernSigner device.

Recommended flow:

1. Create a watch-only wallet on your phone or computer.
2. On KernSigner, display the extended public key or connection QR code.
3. Import the watch-only wallet on your phone/computer.
4. Generate a receiving address on your phone/computer.
5. On KernSigner, navigate to address verification and confirm addresses match.
6. Send a small test amount.

For BTC and Web3 connection tutorials, see [连接钱包教程.md](连接钱包教程.md) (Chinese version).

## BTC Signing

Common flow:

1. Create transaction in watch-only wallet.
2. Watch-only wallet displays PSBT QR code.
3. On KernSigner, select QR scan signing.
4. KernSigner scans the PSBT QR code.
5. Verify recipient address, amount, change, and fees.
6. Enter device PIN.
7. Confirm transaction details again.
8. KernSigner displays signed result QR code.
9. Watch-only wallet scans back and broadcasts.

If any of the recipient address, amount, or fees look incorrect, press Back to cancel immediately.

## Web3 Signing

Common flow:

1. On KernSigner, go to Connect Wallet and select OKX, Bitget, MetaMask, Rabby, or TokenPocket.
2. Choose signing source: local mnemonic or smart card.
3. Scan the QR code on your mobile wallet to add the account.
4. Mobile wallet initiates a test transaction.
5. KernSigner scans the request QR code.
6. Verify chain, address, amount, contract, and authorization info.
7. Choose signing source; if page shows `Enter PIN`, enter device PIN. If page says `Smart Card PIN`, only then enter smart card PIN.
8. Mobile wallet scans back the signed result.

**Note:** TypedData/EIP-712 and other unlaunched capabilities should NOT be advertised as supported features.

## Using a Passphrase (BIP39 Mnemonic Phrase)

A BIP39 passphrase changes your wallet fingerprint and addresses. It is not a "login password" — it is an additional factor that participates in seed derivation. The same mnemonic with different passphrases produces completely different wallets.

Before using, remember:

| Scenario | Result |
| --- | --- |
| Same mnemonic + empty passphrase | One wallet |
| Same mnemonic + `abc` | A different wallet |
| Same mnemonic + `ABC` | Yet another wallet |

If you use a passphrase, you must record both the "mnemonic" and the "passphrase." Losing either means you cannot recover the same wallet.

## Safe Exit

After each session:

1. Return to Home.
2. Clear current session or power off.
3. Unplug USB.
4. Store paper backups securely.
5. Do NOT keep mnemonic phrase photos on your phone.

The device clears sensitive sessions after automatic lock, but do not rely solely on auto-lock.

## New User Acceptance Checklist

When you first receive the device, follow this order:

1. Read [SECURITY_PIN_GUIDE.md](SECURITY_PIN_GUIDE.md) to understand Device PIN vs Smart Card PIN.
2. Create a test mnemonic using `Hex` with all zeros.
3. Verify the mnemonic using an offline BIP39 website.
4. Check raw entropy, fingerprint, and addresses.
5. Perform one backup and recovery drill.
6. Create a watch-only wallet.
7. Send a small test amount.
8. Test PSBT or Web3 signing.
9. Only then decide whether to proceed with real device acceptance.

## Common Issues

| Symptom | Check First |
| --- | --- |
| Don't know which PIN to enter | [SECURITY_PIN_GUIDE.md](SECURITY_PIN_GUIDE.md) |
| Mnemonic doesn't match BIP39 website | [MNEMONIC_CREATION_BIP39_VERIFY.md](MNEMONIC_CREATION_BIP39_VERIFY.md) |
| Wallet connection shows different addresses | [连接钱包教程.md](连接钱包教程.md) (Chinese version) |
| Don't know how to backup | [BACKUP_AND_RECOVERY_GUIDE.md](BACKUP_AND_RECOVERY_GUIDE.md) |
| Camera or QR scanner stuck | [TROUBLESHOOTING_GENERAL.md](TROUBLESHOOTING_GENERAL.md) (Chinese version) |
| Smart card not detected | [TROUBLESHOOTING_SMARTCARD_POWER_OTG.md](TROUBLESHOOTING_SMARTCARD_POWER_OTG.md) |