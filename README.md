[<img align="left" src="https://user-images.githubusercontent.com/52170171/155866095-645d9e43-57d8-4d77-b193-ba1618b75ce5.png" width="300"/>](https://user-images.githubusercontent.com/52170171/155866095-645d9e43-57d8-4d77-b193-ba1618b75ce5.png)

# `parcel`: An ultra-lightweight, cross-platform, end-to-end encrypted, terminal-based group messaging application

[![CodeFactor](https://www.codefactor.io/repository/github/jason-conway/parcel/badge)](https://www.codefactor.io/repository/github/jason-conway/parcel)
[![goto counter](https://img.shields.io/github/search/jason-conway/parcel/goto.svg)](https://github.com/jason-conway/parcel/search?q=goto)
## Supported Platforms

Linux, BSD, Darwin/macOS, Windows, and iOS (*kind of*) on x86-64, x86, AArch32, and AArch64 architectures.

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

The parcel client works without modification on iOS devices using [ish](https://github.com/ish-app/ish). `parceld` will not work due to limitations of `ish`, but it might work with [a-shell](https://github.com/holzschu/a-shell).

### Windows

The simplest way to build parcel on Windows is using [w64devkit](https://github.com/skeeto/w64devkit). The [pre-built *mini* release](https://github.com/skeeto/w64devkit/releases/) variant is just over 50 MB and contains everything needed to build parcel from source.

Once cloned, simply

```console
$ make resources
$ make install
```

Note that, by default, the binaries will be installed at %HOMEPATH%\parcel

#### Windows Binaries

Alternatively, download [prebuilt binaries](https://github.com/jason-conway/parcel/releases/) from Github. Please note, however, that these builds are cross-compiled for x86_64 Windows from an AArch64 MacBook Pro and only **minimally tested** using Wine through Rosetta 2 on an unsupported version of macOS.

## Usage

Print usage information with `-h`:

    usage: parcel [-lhd] [-a ADDR] [-p PORT] [-u NAME]
      -a ADDR  server address (www.example.com, 111.222.333.444)
      -p PORT  server port (default: 2315)
      -u NAME  username displayed alongside sent messages
      -l       use computer login as username
      -h       print this usage information

If the target server is on the default port, then the only required arguments are server address and username. Passing the flag `-l` on launch sets the username as the computer login.

If a required argument is not provided, then it is prompted at startup.

## Security

Parcel encrypts and decrypts message data using [AES128](https://nvlpubs.nist.gov/nistpubs/fips/nist.fips.197.pdf). Messages are authenticated using [CMAC (OMAC1)](https://datatracker.ietf.org/doc/html/rfc4493) to guarantee message authenticity and data integrity. The CMAC tag authenticates ciphertext rather than plaintext, allowing the message to be authenticated prior to decryption.

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

Windows support for UTF-8 has been improving in recent years, with Windows Version 1903 introducing the ability to [set UTF-8 as an active process's codepage](https://docs.microsoft.com/en-us/windows/apps/design/globalizing/use-utf8-code-page). Parcel takes advantage of this and embeds the required XML to set the process codepage directly into the application binary. Additionally, the console's codepage is set to UTF-8 at runtime, although this only works for console output... because despite UTF-8 becoming standarized in 1993, there remains no way to read UTF-8 input in Windows.
To combat this, UTF-16 input is read in one character at a time using `ReadConsoleW()` and encoded as UTF-8 with `WideCharToMultiByte()`. It might not be ideal but gets the job done.

## Client-Side Commands

All commands start with a slash followed by the command itself.

| Command     | Description                             |
| ----------- | --------------------------------------- |
| `/list`     | List available commands                 |
| `/x`        | Exit the server and close parcel        |
| `/username` | Change username                         |
| `/encinfo`  | Display active session and control keys |
| `/file`     | Send a file                             |
| `/clear`    | Clear the screen                        |
| `/version`  | Display application version             |

Commands can be parsed once enough has been typed to become unambiguous, i.e., the username command can be entered as `/u`, clear can be `/c`, etcetera.

Commands are interactive only, no need to supply arguments.


## Wire Format

The wire consists of six sections: mac, lac, iv, length, type, and data

`mac` contains the 16-byte MAC of the IV, length, and data sections.

`lac` contains the 16-byte MAC of `length` only.

`iv` contains the 16-byte Initialization Vector. required for ciper block chaining. Since `data` is encrypted in CBC mode, the IV only needs to be random- not secret, so it is sent as plaintext.

`length` containts the number the bytes in the `data` section.

`type` indicates the type of data contained in the `data` section. 

`data` contains one or more 16-byte chunks of encrypted data

### Wire Types

The possible message types are `TYPE_TEXT`, `TYPE_FILE`, and `TYPE_CTRL`.

The parcel client can send messages of type `TYPE_TEXT` and `TYPE_FILE`. Only the parcel daemon can send `TYPE_CTRL`, which are used to trigger a GDHKD sequence to update the key.

### Type-Specific Section Layout

#### TYPE_CTRL

When a message has the type `TYPE_CTRL`, the `data` section will contain a populated `wire_ctrl_message` struct- containing a control function, function arguments, and the renewed control key.

#### TYPE_FILE

When a message has the type `TYPE_FILE`, the `data` section will contain a populated `wire_file_message` struct- containing the filename, the file size, and the file data.
