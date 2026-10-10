# Changing Givoice settings with an agent

The complete, offline settings guide is [readme.txt](readme.txt). The same file is
included in installed apps:

- macOS: `Givoice.app/Contents/Resources/readme.txt`
- Windows: `readme.txt` beside `Givoice.exe` (created on first launch)
- Linux .deb: `/usr/share/doc/givoice/readme.txt`
- Linux local install: `<install prefix>/share/doc/givoice/readme.txt`

It describes actual per-user settings locations, platform-specific keys, supported
values, examples, and the quit/edit/relaunch procedure. Repository access is not
required. Keep `readme.txt` consistent with all three platform settings loaders.

macOS/Windows support `transcription_engine = "openai" | "elevenlabs" | "groq"`;
legacy `auto` keeps API-key-prefix selection. Linux continues to select by API
key. Each provider's credentials and model are retained when switching engines.
Legacy `system` settings switch to the provider matching a saved API key (or an
environment key); without a key, OpenAI is selected. The system engine is no longer
offered in Settings.
