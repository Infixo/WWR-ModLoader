# Worldwide Rush ModLoader
A custom-built mod loader for the [Worldwide Rush game](https://store.steampowered.com/app/3325500/Worldwide_Rush/).

## Features

### Setup and Usage
- Mods need to be 64-bit .Net 8.0 compatible assemblies.
- Place the mods in the game_dir\Mods\ folder. Create one if necessary.
- Register mod names in the config file Bootstrapper.txt (sample provided).
- The Loader i.e. Injector.exe and Bootstrapper.dll with related files must be in the same directory, anywhere you like.
- Run Injector.exe.
  - If you run from cmd, then you must be in the directory where loader is placed. 
  - For the first run it is recommended to do it from command prompt.

### Troubleshooting
- Output messages are logged into wwrmodloader.txt in the %TEMP% dir. It is usually C:\Users\<Username>\AppData\Local\Temp.

## Technical

### Requirements and Compatibility
- 64-bit Windows.

### Known Issues
- Nothing atm.

### Changelog
- v1.0.1 (2025-10-09)
  - Added thread exit code handling.
- v1.0.0 (2025-10-02)
  - Initial release.

### Support
- Please report bugs and issues on [GitHub](https://github.com/Infixo/WWR-ModLoader).
- You may also leave comments on [Discord](https://discord.com/channels/1342565384066170964/1421898965556920342).
