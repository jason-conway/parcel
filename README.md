[<img align="left" src="https://user-images.githubusercontent.com/52170171/155866095-645d9e43-57d8-4d77-b193-ba1618b75ce5.png" width="300"/>](https://user-images.githubusercontent.com/52170171/155866095-645d9e43-57d8-4d77-b193-ba1618b75ce5.png)

# `parcel`: An ultra-lightweight, cross-platform, end-to-end encrypted, terminal-based group messaging application

## Supported Platforms

Linux, BSD, Darwin/macOS, Windows, and iOS (*kind of*)

## Installation

### Linux, BSD, Darwin/macOS

After cloning the repository, simply

```console
$ make install
```

to build and install both `parcel` and `parceld` under `/usr/local/bin`.

Optionally, `PREFIX` can be set in order to change the install location.

```console
$ make PREFIX=~/.parcel install
```

#### iOS

The parcel client works without modification on iOS devices using [ish](https://github.com/ish-app/ish). `parceld` does not work as of this version.

### Windows

The simplest way to build parcel on Windows is using [w64devkit](https://github.com/skeeto/w64devkit). The [pre-built *mini* release](https://github.com/skeeto/w64devkit/releases/) variant is just over 50 MB and contains everything needed to build from source following the same steps shown above.

Note that, by default, the binaries will be installed at %HOMEPATH%\parcel

#### Windows Binaries

Alternatively, download [prebuilt binaries](https://github.com/jason-conway/parcel/releases/) from Github. Please note, however, that these builds are cross-compiled for x86_64 Windows from an AArch64 MacBook Pro and only **minimally tested** using Wine through Rosetta 2 on an unsupported version of macOS.

## Usage

Print usage information with `-h`:

    usage: parcel [-a ADDR] [-p PORT] [-u NAME]
      -a ADDR  server address (www.example.com, 111.222.333.444)
      -p PORT  server port (3724, 9216)
      -u NAME  username displayed alongside sent messages
      -h       print this usage information

## Security

Parcel encrypts and decrypts message data using AES128. Messages are authenticated using CMAC (OMAC1) to guarantee message authenticity and data integrity. The CMAC tag authenticates ciphertext rather than plaintext, allowing the message to be authenticated prior to decryption.

### Key Exchange

The parcel daemon, `parceld`, generates a random 32-byte control key at startup. After establishing a secured channel, this key is shared with the client and used to decrypt `TYPE_CTRL` messages from the daemon. 

The control key is used only once, with `TYPE_CTRL` messages containing the next control keys as part of its encrypted contents.

Upon recieving and decrypting a control message, the parcel clients perform a multi-party [Elliptic-curve Diffie-Hellman](https://en.wikipedia.org/wiki/Elliptic-curve_Diffie%E2%80%93Hellman) key exchange using [Curve25519](https://en.wikipedia.org/wiki/Curve25519), at which point all clients posess a new shared key.

A new group key is derived whenever the number of clients changes.

## Frequently Asked Questions

> But why though?

## Windows Considerations

`parcel` uses standard [ANSI X3.64 (ISO 6429)](https://nvlpubs.nist.gov/nistpubs/Legacy/FIPS/fipspub86.pdf) escape sequences for in-band signaling. Despite having been standardized since 1979, they lacked proper support in Windows until the Windows 10 Anniversery Update in 2016. If you're running this version or newer, parcel *should* properly configure the console automatically. In the unlikely case that parcel is unable to configure the console, you can try adding the following registry key to [globally enable Virtual Terminal Processing](https://superuser.com/questions/413073/windows-console-with-ansi-colors-handling).

```ps1
REG ADD HKCU\CONSOLE /f /v VirtualTerminalLevel /t REG_DWORD /d 1
```

If an earlier version of Windows is being used or if parcel is being run from a different shell, the extra console configuration will fail silently and parcel will continue as normal.

The number of active clients supported by `parceld` is determined by `FD_SETSIZE`. As a result, `parceld` supports a maximum of 64 active clients when running on Windows.

## Client-Side Commands

All commands start with a colon followed by the command itself.

| Command        | Description                           |
| -------------- | ------------------------------------- |
| `:q`           | Cleanly quit parcel                   |
| `:file`        | Send a file                           |
| `:fingerprint` | Display fingerprint of the public key |
| `:username`    | Change client username                |


## Wire Format

The wire consists of 4 sections: mac, iv, length, data

`mac` contains the 16-byte MAC of the IV, length, and data sections. 

`iv` contains the 16-byte Initialization Vector. required for ciper block chaining. Since `data` is encrypted in CBC mode, the IV only needs to be random- not secret, so it is sent as plaintext.

`length` containts the number the bytes in the `data` section.

`data_type` indicates the type of data contained in the `data` section. 

`data` contains one or more 16-byte chunks of encrypted data

### Wire Types

The possible message types are `TYPE_TEXT`, `TYPE_FILE`, and `TYPE_CTRL`.

The parcel client can send messages of type `TYPE_TEXT` and `TYPE_FILE`. Only the parcel daemon can send `TYPE_CTRL`, which are used to trigger a GDHKD sequence to update the key.

### Type-Specific `data` Section Layout

When a message has the type `TYPE_CTRL`, the `data` section will always be 48-bytes long.

The first 8 bytes encode the value `GHDR`, representing the number of intermediate shared-key derivation rounds to perform.

The second 8 bytes are set to `\0`

The last 32 bytes are set to the next control message key.
