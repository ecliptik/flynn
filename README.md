# Flynn

A Telnet client for classic Macintosh (68000/Mac Plus), targeting System 6.0.8 with MacTCP 2.1. Cross-compiled on Linux using [Retro68](https://github.com/autc04/Retro68).

![Flynn connected to a Linux server via telnet, showing neofetch output on a Macintosh Plus running System 6.0.8](docs/screenshots/flynn-telnet-session.png)

## Features

- Client-side Telnet protocol with IAC negotiation (BINARY, ECHO, SGA, TTYPE, NAWS)
- VT100 terminal emulation (cursor movement, screen clearing, text attributes)
- MacTCP networking for TCP/IP connectivity
- Designed for interactive terminal use (vi, nano, shell sessions)
- Scrollback viewing with Cmd+Up/Down keyboard navigation
- Copy/paste via Mac clipboard (Cmd+C to copy screen, Cmd+V to paste)
- Settings persistence (host/port saved across launches)
- Option key as Ctrl modifier for M0110 keyboard
- Proper About dialog with version and credits
- Targets Motorola 68000 CPU (Mac Plus compatible)

## Current Status

Phases 0-6 are complete. Flynn is a fully featured telnet client: connects to real servers, renders VT100 terminal output including full-screen TUI applications (nano, vi), handles interactive shell sessions with copy/paste and scrollback viewing, saves preferences across launches, and supports the M0110 keyboard's Option key as Ctrl. The application runs on a Macintosh Plus with 4MB RAM under System 6.0.8 with MacTCP 2.1.

## Building

Requires the [Retro68](https://github.com/autc04/Retro68) cross-compilation toolchain. Build it from source (68k only):

```bash
git clone https://github.com/autc04/Retro68.git
cd Retro68 && git submodule update --init && cd ..
mkdir Retro68-build && cd Retro68-build
bash ../Retro68/build-toolchain.bash --no-ppc --no-carbon --prefix=$(pwd)/toolchain
```

Then build Flynn:

```bash
./build.sh
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
