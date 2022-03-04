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

The parcel daemon, `parceld`, generates a random session key at startup.

As of version 0.9.0, this key is static over the duration of the session. This behavior is planned on being replaced with a ratcheting mechanism for forward secrecy.

Curve25519 is used to perform the [Elliptic-curve Diffie-Hellmans](https://en.wikipedia.org/wiki/Elliptic-curve_Diffie%E2%80%93Hellman) between the client and server. 


## Frequently Asked Questions

> But why though?

## Windows Considerations

`parcel` uses standard [ANSI X3.64 (ISO 6429)](https://nvlpubs.nist.gov/nistpubs/Legacy/FIPS/fipspub86.pdf) escape sequences for in-band signaling. Despite having been standardized since 1979, they lacked proper support in Windows until the Windows 10 Anniversery Update in 2016. If you're running this version or newer, parcel *should* properly configure the console automatically. In the unlikely case that parcel is unable to configure the console, you can try adding the following registry key to [globally enable Virtual Terminal Processing](https://superuser.com/questions/413073/windows-console-with-ansi-colors-handling).

```ps1
REG ADD HKCU\CONSOLE /f /v VirtualTerminalLevel /t REG_DWORD /d 1
```

If an earlier version of Windows is being used or if parcel is being run from a different shell, the extra console configuration will fail silently and parcel will continue as normal.

## Wire Format

The wire consists of 4 sections: mac, iv, length, data

`mac` contains the 16-byte MAC of the IV, length, and data sections. 

`iv` contains the 16-byte Initialization Vector. required for ciper block chaining. Since `data` is encrypted in CBC mode, the IV only needs to be random- not secret, so it is sent as plaintext.

`length` containts the number the bytes in the `data` section. 

`data` contains one or more 16-byte chunks of encrypted data
