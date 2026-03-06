# Flynn

A Telnet client for classic Macintosh (68000/Mac Plus), targeting System 6.0.8 with MacTCP 2.1. Cross-compiled on Linux using [Retro68](https://github.com/autc04/Retro68).

## Features

- Client-side Telnet protocol with IAC negotiation (BINARY, ECHO, SGA, TTYPE, NAWS)
- VT100 terminal emulation (cursor movement, screen clearing, text attributes)
- MacTCP networking for TCP/IP connectivity
- Designed for interactive terminal use (vi, nano, shell sessions)
- Targets Motorola 68000 CPU (Mac Plus compatible)

## Building

Requires the [Retro68](https://github.com/autc04/Retro68) cross-compilation toolchain installed on Linux.

```bash
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=/path/to/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake
make
```

## Testing

Uses [Snow](https://snowemu.com/) emulator (v1.3.1) with a Mac Plus ROM and System 6.0.8 SCSI hard drive image. Snow supports DaynaPORT SCSI/Link Ethernet emulation for MacTCP networking. The emulator can be fully automated via X11 for unattended testing. See `docs/TESTING.md` for details.

## Acknowledgments

This project incorporates code from two open-source classic Macintosh applications by joshua stein (jcs@jcs.org), both released under the ISC license:

- **[wallops](https://github.com/jcs/wallops)** - IRC client for classic Macintosh. MacTCP wrapper (`tcp.c`/`tcp.h`), DNS resolution (`dnr.c`/`dnr.h`), utility functions (`util.c`/`util.h`), and `MacTCP.h` are used directly from this project.
- **[subtext](https://github.com/jcs/subtext)** - BBS server for classic Macintosh. The Telnet IAC protocol implementation (`telnet.c`/`telnet.h`) served as the reference for this project's client-side telnet engine.

See the individual source files for copyright notices and license terms.

## License

ISC License. See [LICENSE](LICENSE) for full details.
