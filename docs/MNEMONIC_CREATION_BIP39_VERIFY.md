# Mnemonic Creation and BIP39 Website Verification Guide

This document is for first-time users. The goal: after generating a mnemonic phrase using the creation methods on your device, you can use a BIP39 website to verify that "the same entropy produces the same mnemonic phrase."

**Important reminder:** Do NOT enter real wallet mnemonics into internet-connected web pages. For real wallet verification, use an offline BIP39 tool, or only practice with test mnemonics.

## Remember These Three Sentences First

- Mnemonic phrases are NOT randomly arranged words. BIP39 first generates "raw entropy," then automatically adds checksum bits, and finally converts them into 12/15/18/21/24 English words.
- As long as the "raw entropy" and "English word list" are the same, KernSigner and the BIP39 website should generate identical mnemonic phrases.
- The most reliable verification method: view the hex raw entropy on KernSigner, paste it into the BIP39 website's Entropy field, select `Use Raw Entropy` for Mnemonic Length.

## Creation Entry

Common path:

1. Go to Home page.
2. Tap `Mnemonic`.
3. Tap `Create`.
4. Select creation method, e.g., `Dice`, `Coin Flip`, `Manual`, `Camera`, `Hex`, `D20`, `Playing Cards`, `Smart Card`.
5. Follow on-screen prompts to select word count and input random materials.
6. After generation, carefully write down the mnemonic phrase and complete the confirmation flow on device.

Beginners are recommended to优先 practice with `12 words`. Before entering production use in the future, you may choose `24 words` for higher entropy strength; current test fund acceptance version only uses test mnemonics, testnet assets, or small acceptance funds you can explicitly afford to lose.

## Word Count and Input Length

| Word Count | Raw Entropy | Hex Length | Coin Flip Length |
| --- | ---: | ---: | ---: |
| 12 words | 128 bits | 32 hex characters | 128 0/1s |
| 15 words | 160 bits | 40 hex characters | 160 0/1s |
| 18 words | 192 bits | 48 hex characters | 192 0/1s |
| 21 words | 224 bits | 56 hex characters | 224 0/1s |
| 24 words | 256 bits | 64 hex characters | 256 0/1s |

Hex characters only allow `0-9` and `a-f`. Case insensitive.

## BIP39 Website Offline Preparation

Recommended tool is Ian Coleman's BIP39 tool.

1. Open project address: `https://github.com/iancoleman/bip39`
2. Download `bip39-standalone.html` from the release page.
3. Disconnect from internet.
4. Double-click to open `bip39-standalone.html`.
5. Select `English` in the page.
6. Check `Show entropy details` to display the Entropy area.

When only verifying test mnemonics, you can also open the online version: `https://iancoleman.io/bip39/`. Do NOT do this with real mnemonics.

## Universal Verification Method: Verify Using Raw Entropy

This method works for all creation methods, including Camera, D20, Smart Card, and Manual input.

### Step 1: View Raw Entropy on KernSigner

After creating and loading the mnemonic phrase:

1. Return to Home page.
2. Tap `Mnemonic`.
3. Enter the currently loaded mnemonic menu.
4. Tap `Raw Entropy`.
5. Record the continuous hex string at the bottom of the page.

The page may also display:

- `Raw Input`: your dice, coin, playing cards input materials.
- `Hex`: the BIP39 raw entropy used to generate the mnemonic phrase.

Prioritize using the `Hex` line for verification.

### Step 2: Enter Raw Entropy on BIP39 Website

1. Open offline BIP39 website.
2. Check `Show entropy details`.
3. Find the `Entropy` input box.
4. Paste KernSigner's hex raw entropy into it; spaces can be removed.
5. Confirm `Entropy Type` is set to `hexadecimal`.
6. Select `Use Raw Entropy (3 words per 32 bits)` for `Mnemonic Length`.
7. View `BIP39 Mnemonic`.
8. The mnemonic shown on the website should be exactly identical to KernSigner's.

Note: When verifying KernSigner's raw entropy, do NOT select `12 Words`, `15 Words`, `24 Words` options on the BIP39 website. Those options make the website SHA256 the input first then truncate, producing different results.

## Method 1: Hex Creation

Suitable for people who want the most direct comparison with the BIP39 website.

### How to Do It on KernSigner

1. Go to `Mnemonic` -> `Create` -> `Hex`.
2. Select word count.
3. Enter hex of corresponding length:
   - 12 words: enter 32 characters.
   - 24 words: enter 64 characters.
4. Tap `Generate`.
5. Verify and confirm the mnemonic phrase.

### How to Verify on BIP39 Website

1. Check `Show entropy details`.
2. Enter the same hex string in `Entropy`.
3. Select `hexadecimal` for `Entropy Type`.
4. Select `Use Raw Entropy` for `Mnemonic Length`.
5. The website's `BIP39 Mnemonic` must match KernSigner's exactly.

### Getting Started Test

Test with 12 words:

```text
00000000000000000000000000000000
```

Expected mnemonic:

```text
abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about
```

This is only a test vector; do NOT use it as a real wallet.

## Method 2: Coin Flip

Suitable for creating with purely physical randomness.

### How to Do It on KernSigner

1. Go to `Mnemonic` -> `Create` -> `Coin Flip`.
2. Select word count.
3. Agree heads = `1`, tails = `0`, or vice versa, but be consistent throughout.
4. After each flip, enter a `0` or `1` on the device.
5. When the page indicates target bits reached, tap `Done` or `Generate`.

### How to Verify on BIP39 Website

1. Enter the same `0/1` string in `Entropy`.
2. Select `binary` for `Entropy Type`.
3. Select `Use Raw Entropy` for `Mnemonic Length`.
4. Compare `BIP39 Mnemonic`.

Common error: adding one extra 0 or 1 will completely change subsequent mnemonics.

## Method 3: Six-Sided Dice

Suitable for creating with regular dice.

### How to Do It on KernSigner

1. Go to `Mnemonic` -> `Create` -> `Dice`.
2. Select word count.
3. Roll six-sided dice, enter `1-6` in order.
4. The page shows whether entropy is sufficient.
5. When sufficient, tap `Done` or `Generate`.

KernSigner uses the same unbiased digit extraction rules as Ian Coleman BIP39 tool:

| Dice Value | Converted Bit |
| --- | --- |
| 1 | `01` |
| 2 | `10` |
| 3 | `11` |
| 4 | `0` |
| 5 | `1` |
| 6 | `00` |

### How to Verify on BIP39 Website

Most reliable method: verify using KernSigner's raw entropy hex.

You can also verify directly with dice sequence:

1. Enter the same dice sequence in `Entropy`, e.g., `123456123456...`.
2. Select `dice` for `Entropy Type`.
3. Select `Use Raw Entropy` for `Mnemonic Length`.
4. Compare the mnemonic generated by the website.

If direct dice sequence verification fails, first check whether you entered extra dice rolls. KernSigner takes only the entropy needed for the selected word count; for beginners, verifying with raw entropy hex is less error-prone.

## Method 4: Twenty-Sided Dice (D20)

Suitable for users who have D20 dice.

### How to Do It on KernSigner

1. Go to `Mnemonic` -> `Create` -> `D20` or `Twenty-Sided Dice`.
2. Select word count.
3. Each roll, enter a number between `1-20`.
4. Separate numbers with spaces, e.g.:

```text
1 20 7 13 4 9 18 2
```

5. When the page indicates sufficient, tap `Generate`.

### How to Verify on BIP39 Website

The BIP39 website has no native D20 type, so do NOT directly paste D20 numbers into Entropy.

Correct method:

1. Open `Raw Entropy` on KernSigner.
2. Copy the hex raw entropy.
3. Enter this hex string in BIP39 website's `Entropy`.
4. Select `hexadecimal` for `Entropy Type`.
5. Select `Use Raw Entropy` for `Mnemonic Length`.
6. Compare mnemonics.

## Method 5: Playing Cards

Suitable for creating with shuffled physical playing cards.

### How to Do It on KernSigner

1. Go to `Mnemonic` -> `Create` -> `Playing Cards`.
2. Select word count.
3. Shuffle and enter from top of deck in order.
4. Card format is "rank + suit":
   - `A` represents Ace.
   - `T` represents 10.
   - `J/Q/K` represent J/Q/K.
   - `C` Clubs, `D` Diamonds, `H` Hearts, `S` Spades.
5. Example:

```text
AH QS 9D TC 2C
```

### How to Verify on BIP39 Website

Most reliable method: verify using KernSigner's raw entropy hex.

You can also verify directly with playing card sequence:

1. Enter the same cards in `Entropy`, e.g., `AH QS 9D TC`.
2. Select `card` for `Entropy Type`.
3. Select `Use Raw Entropy` for `Mnemonic Length`.
4. Compare mnemonics.

Note: `10` must be written as `T`, NOT `10H`. E.g., heart 10 is `TH`.

## Method 6: Camera

Suitable when you don't want to manually input lots of random materials.

### How to Do It on KernSigner

1. Go to `Mnemonic` -> `Create` -> `Camera`.
2. Select word count.
3. Photograph scenes with rich details, e.g., desktops, paper textures, different colored objects.
4. Tap screen to capture.
5. If randomness is insufficient, switch to a more complex image and retake.
6. Device shows snapshot SHA256, then tap `Continue`.

### How to Verify on BIP39 Website

Camera method essentially:

1. Take photo to get image.
2. Calculate SHA256 of snapshot.
3. Take entropy of needed length based on word count.
4. Generate mnemonic using BIP39.

For verification, recommend directly using KernSigner's raw entropy hex:

1. After creation completes, go to `Raw Entropy`.
2. Copy the hex string.
3. On BIP39 website select `hexadecimal` + `Use Raw Entropy`.
4. Compare mnemonics.

If you only see snapshot SHA256:

- 12 words: take first 32 hex characters of SHA256.
- 15 words: take first 40 hex characters.
- 18 words: take first 48 hex characters.
- 21 words: take first 56 hex characters.
- 24 words: use full 64 hex characters.

Then verify on BIP39 website following the `Hex Creation` method.

## Method 7: Manual Word Selection

Suitable for people who already know BIP39 words, or need to restore/check a word set.

### How to Do It on KernSigner

1. Go to `Mnemonic` -> `Create` -> `Manual`.
2. Enter or select words in order.
3. The last word contains checksum bits; do NOT fill randomly.
4. If device reports invalid, usually the word is misspelled, wrong order, or last word checksum is incorrect.

### How to Verify on BIP39 Website

1. On BIP39 website find `BIP39 Mnemonic`.
2. Paste complete mnemonic phrase.
3. Select language as `English`.
4. If page has no invalid warning, BIP39 validation passes.
5. You can also check `Show entropy details` to view corresponding raw entropy.

Manual method is NOT "auto-create from random input," so verification focus is whether the mnemonic itself passes BIP39 checksum.

## Method 8: Smart Card

Suitable for using SeedKeeper/Satochip etc. smart card randomness capabilities.

### How to Do It on KernSigner

1. Connect smart card and card reader.
2. Go to `Mnemonic` -> `Create` -> `Smart Card`.
3. Enter smart card PIN as prompted.
4. Select word count or use default.
5. Device generates random secret from smart card, then converts to BIP39 mnemonic phrase.

### How to Verify on BIP39 Website

1. After creation and loading, go to KernSigner's `Raw Entropy`.
2. Copy the hex string.
3. On BIP39 website select `hexadecimal` + `Use Raw Entropy`.
4. Compare mnemonics.

If page only shows mnemonic without smart card randomness, you can also paste the mnemonic into the `BIP39 Mnemonic` field to check validation.

## Common BIP39 Website Settings

When verifying KernSigner generation results, generally use these settings:

| Item | Should Select |
| --- | --- |
| Mnemonic Language | `English` |
| Show entropy details | Checked |
| Entropy Type | Select by input: `hexadecimal` / `binary` / `dice` / `card` |
| Mnemonic Length | `Use Raw Entropy` |
| BIP39 Passphrase | Leave empty when verifying mnemonic itself |

If only checking mnemonic validity:

1. Paste mnemonic into `BIP39 Mnemonic`.
2. No need to fill Entropy.
3. If page reports no invalid, validation passes.

To compare addresses, also ensure network and path match. E.g., BTC native SegWit common path is `m/84'/0'/0'`, testnet is `m/84'/1'/0'`. Different paths produce different addresses.

## Troubleshooting Inconsistencies

Check in this order:

1. Whether you used offline version or the same BIP39 tool page.
2. Whether `Mnemonic Language` is `English`.
3. Whether `Mnemonic Length` selected `Use Raw Entropy`.
4. Whether hex string is missing characters or has symbols other than spaces.
5. Whether coin flip rules were reversed mid-way.
6. Whether dice was over-entered or under-entered.
7. Whether playing card 10s were written as `T`.
8. Whether D20 numbers were directly pasted into website. D20 must use KernSigner's hex raw entropy for verification.
9. Whether wrong SHA256 was used for camera method: 12 words only uses first 32 hex characters, 24 words uses full 64.
10. Whether passphrase was included. When verifying mnemonic itself, leave passphrase empty.

## Most Recommended Beginner Flow

First practice:

1. Use `Hex Creation`.
2. Enter all-zero test entropy:

```text
00000000000000000000000000000000
```

3. See test mnemonic:

```text
abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about
```

4. Verify on BIP39 website using `hexadecimal` + `Use Raw Entropy`.
5. After getting familiar, create new test mnemonics with dice, coin, camera, or smart card for recovery drills.
6. In the future when creating real mnemonics: do NOT verify online, do NOT take photos, do NOT send to anyone; do NOT import real mnemonics before firmware passes production gate.

## Reference Materials

- BIP39 Official Specification: `https://github.com/bitcoin/bips/blob/master/bip-0039.mediawiki`
- Ian Coleman BIP39 Tool: `https://iancoleman.io/bip39/`
- Ian Coleman BIP39 Source Code and Offline Instructions: `https://github.com/iancoleman/bip39`
- Ian Coleman Entropy Support Types Source: `https://github.com/iancoleman/bip39/blob/master/src/js/entropy.js`