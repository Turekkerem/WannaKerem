# Wannakerem – A WannaCry Parody PoC

Let's be real – the WannaCry outbreak in 2017 was one of those rare moments that permanently reshaped how we think about network security. Two 0-days (EternalBlue and DoublePulsar) in a single worm? Absolute mayhem. I've always been fascinated by how that specific combo worked, not because of the damage it caused, but because of the sheer audacity of its architecture.

Funny enough, my own personal machine got hit by WannaCry back then. Luckily, an active AV caught it just in time before it could do serious harm – but that creepy skull dialog and the ticking ransom timer stuck with me. So, I decided to build my own take on it: **Wannakerem**.

It's a purely academic (but still weaponized) PoC that mimics the visual flair and cryptographic backbone of WannaCry, without any of the worm-like propagation or real-world malicious intent. Think of it as a respectful (and slightly nostalgic) reverse-engineering deep-dive.

---

## Technical Deep-Dive

### 1. Cryptographic Core – AES-256-GCM
All file data is encrypted using **AES-256 in GCM (Galois/Counter Mode)**. I chose GCM over the older CBC because it provides built-in authentication – so not only is the data scrambled, but any tampering with ciphertext gets instantly detected during decryption.

The implementation relies entirely on **OpenSSL** – battle-tested, widely audited, and surprisingly pleasant to work with. The Master Key and nonces are generated using OpenSSL's **CSPRNG (`RAND_bytes`)**, ensuring proper entropy without relying on dodgy custom RNGs.

### 2. Protecting Keys in RAM – `SecureBuffer`
One of the first things real malware analysts do is dump the process memory to extract encryption keys. To make that significantly harder, Wannakerem uses a custom `SecureBuffer` class that:

- Locks the memory page in RAM with `VirtualLock` – preventing the OS from swapping it to disk (bye-bye pagefile.sys leaks).
- Explicitly zeroes out the buffer using `SecureZeroMemory` before releasing it – no leftover sensitive data floating around in heap fragments.
- Implements proper move semantics (and deletes copy semantics) to avoid accidental duplication of sensitive material.
- The result? Even if you attach a debugger or take a memory dump, grabbing the actual Master Key becomes a real pain.

### 3. Key Storage – RSA-8192 with OAEP
The symmetric Master Key never touches the disk in plaintext. Instead:

- It is wrapped using a **hardcoded RSA public key (8192 bits)**.
- The padding scheme is **OAEP** (Optimal Asymmetric Encryption Padding) – which adds necessary randomization and protects against certain chosen-plaintext attacks.
- The wrapped key is saved as `masterkey.enc`.
- A SHA-256 hash of the original Master Key is stored alongside it in `masterkey.sha256`. This hash acts as a lightweight checksum to verify if the user (or analyst) entered the correct key during the recovery phase.

### 4. Preventing Double-Encryption – `marker.dat`
In chaotic environments (or if someone accidentally runs the binary twice), you risk encrypting already-encrypted files – which usually corrupts them beyond recovery. To avoid that footgun, Wannakerem drops a `marker.dat` file directly in `C:\`.

On startup, the program checks for this marker. If it exists, the encryption routine gracefully bails out. This keeps the test environment predictable and prevents accidental data corruption during repeated lab runs.

---

## Known Limitations – Honest Confession Time

Let's address the elephant in the room: **the main AES-GCM branch is a bit... temperamental.**

Here's the unfiltered truth:

- The encryption part works perfectly. Files get scrambled, markers get dropped, the skull shows up. 
- **Decryption, however, hits a wall.** Due to a subtle but annoying bug in how `SecureBuffer` hands off the Master Key to the hex conversion routine (and the subsequent SHA‑256 validation), the program will mark **every single key you enter as invalid** – even if you copy it directly from `masterkey.sha256`. The GCM side of things technically *can* decrypt, but the validation gatekeeper refuses to let you through.

So, if you're planning to test the full "pay the ransom and get your files back" flow, you're going to end up frustrated (or convinced you've mistyped the key about fifty times).

**Now for the plot twist:** The completely different, deprecated sibling living in `old_very_cryptographically_insecure` – the one using **AES-CBC with a user-supplied password** – works flawlessly. Encrypt? Yes. Decrypt? Absolutely. Accepts literally any password you throw at it? No,but you are surprised

Why? Because that version has none of the `SecureBuffer` / hex-conversion complexity. It takes your plaintext password, derives a key, and just does its job without overthinking it. No RSA wrapping, no memory locking, no SHA‑256 gatekeeping – just straightforward, slightly old‑school file encryption.

The takeaway? This main branch is a fascinating but **intentionally fragile** gem. It shows you *how* ransomware architects think (GCM, RSA wrapping, memory protection), but also proves that even smart ideas can break at the plumbing level. Treat it as a brain-teaser for reverse engineers and a cautionary tale about over‑engineering validation flows.

---

## Visuals & Atmosphere – A Nod to the Old School

I'll be honest – the visual part was the most fun. Wannakerem changes the desktop wallpaper to the classic **"YOUR FILES HAS BEEN ENCRYPTED"** screen, spawns dialog windows reminiscent of the original WannaCry UI, and includes a ticking timer for dramatic effect.

The floating skulls and creepy vibe? That's heavily inspired by **"Welcome to the Game 2"** – a game that absolutely nails the aesthetic of old-school dark-web hacking. I've always loved that atmosphere, and mashing it up with this cryptographic PoC just felt right.
<img width="998" height="705" alt="image" src="https://github.com/user-attachments/assets/c1fe6dc0-70b8-4825-bca4-456e158d820e" />
<img width="1017" height="698" alt="image" src="https://github.com/user-attachments/assets/7243a514-fd81-4e1c-9613-0bc2a2c44914" />
<img width="1013" height="706" alt="image" src="https://github.com/user-attachments/assets/898b6aea-56cf-40cf-89d4-1c7ddd9d2bbf" />



---

## Project Structure (Modules)

The source is split into logical modules to keep things maintainable:

| File | Purpose |
| :--- | :--- |
| **common.hpp** | The central header. Contains all class declarations (`SecureBuffer`, `ThreadPool`), constants (key sizes, buffer lengths), and function prototypes. It's the blueprint for the entire project. |
| **symmetric.cpp** | The heavy lifter. Implements AES-256-GCM streaming encryption/decryption (`EncryptFileStream` / `DecryptFileStream`). Uses a 64KB rolling buffer, so even huge files (multiple GBs) can be processed without hogging all your RAM. |
| **asymmetric.cpp** | Handles the RSA logic. Loads the hardcoded decimal-modulus public key, creates the `RSA*` structure, and performs the OAEP wrapping/unwrapping of the Master Key. |
| **other_functions.cpp** | A mixed bag of utilities: file permission checks (`HasReadWriteAccess`), secure deletion (`SecureDeleteFile`), system path resolution (Desktop, markers), and the SHA-256 hashing logic for key verification. |
| **visual.cpp** | Pure WinAPI wizardry. Creates the dialogs, paints the skull, switches the wallpaper, and manages the overall GUI flow. |

> *Note: `classes.cpp` and `secrets.cpp` are now deprecated and have been removed from the project – they were early prototypes that didn't survive the refactoring (and honestly, having the public key in a dedicated file called `secrets.cpp` was just asking for trouble).*

---

## Compilation Guide

The project builds on Windows using **MinGW** (or MSVC with adjustments). You'll need OpenSSL libraries linked properly.

**Full build command (tested and working):**

`g++ -std=c++17 -O2 -IModules Modules/asymmetric.cpp Modules/symmetric.cpp Modules/other_functions.cpp Modules/visual.cpp ransomware.cpp resource.res -mwindows -lcrypto -lssl -Wdeprecated-declarations -lws2_32 -lgdi32 -lcrypt32 -lshlwapi -lstdc++fs -lole32 -lshell32 -luuid -lwinmm -static -o ransomware.exe`

**Optional audio support (resource file):**  
If you want the laugh effects, make sure `resource.res` is compiled and placed in the project root before running the above command.

*Copyright note:* I'm not shipping actual `.wav` or `.mp3` files in this repo due to licensing restrictions. You'll need to supply your own royalty-free sound files if you want the full "cinematic" experience.

---

## Bonus: Legacy Module – `old_very_cryptographically_insecure`

Alright, let's address the elephant in the repo. There's a folder called `old_very_cryptographically_insecure` – and yes, that name is intentionally dramatic.

Inside, you'll find two completely different beasts:

1.  **The AES-CBC experiment with a hardcoded password** – This was my very first stab at file encryption. Using AES-CBC isn't inherently evil, but pairing it with a *hardcoded, static key* absolutely is. Do not use this. Do not copy this. It exists purely as a historical relic to show how *not* to do cryptography. I kept it around as a cautionary tale for my future self (and for anyone curious about the evolution of the project).

2.  **A surprisingly useful, fully legitimate derivative** – Take the same core logic, strip out all the ransomware UI, add a clean file-selection dialog, and let the user type their *own* password... and suddenly you have a perfectly legal, genuinely handy file-encryption tool for personal backups or privacy.

The catch? This derivative completely lacks `SecureBuffer` and any RSA key wrapping. The password sits in plaintext memory, and there's no memory locking or zeroing. So while it's perfectly fine for zipping up your own documents on your own machine, it's not built for adversarial environments. Think of it as the "gentle introduction" to the main project – but please, stick to the main `Wannakerem` branch for anything security-critical.

---

## Disclaimer
[WARNING]
> This code is strictly for **educational, academic, and authorized Red/Blue team training** inside **isolated virtual machines** with network adapters disabled and snapshots enabled.
>
> It does not propagate over networks, does not exploit any vulnerabilities, and contains built-in safeguards (`marker.dat`, memory protection) to prevent accidental damage.
>
> The author does not condone or support the use of this software outside of controlled laboratory environments. You are responsible for how you use this code.

---

## Why This Stands Out on a Red Team CV

This project isn't just "another ransomware PoC" – it demonstrates:

- **Deep applied cryptography** – You know the difference between GCM and CBC, why OAEP matters, and how to integrate OpenSSL properly in a Windows environment.
- **WinAPI & memory security** – You understand `VirtualLock`, `SecureZeroMemory`, and why keys lingering in RAM are a huge deal.
- **Historical threat awareness** – You studied WannaCry inside out and can break down its technical impact to a non-technical audience.
- **Good engineering hygiene** – Thread pools, streaming I/O, and marker-based guards show you think about edge cases and reliability, not just blowing things up.

It's professional, it's quirky, and it proves you actually know your stuff beyond just running Metasploit modules.

Happy hacking – and keep those snapshots ready.
