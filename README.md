<p align="center">
  <a href="https://nightly.link/mistakesmultiplied/neptune/workflows/msbuild/master/Amalgamx64Release.zip">
    <img src=".github/assets/download.png" alt="Download" width="auto" height="auto">
  </a>
  <a href="https://nightly.link/mistakesmultiplied/neptune/workflows/msbuild/master/Amalgamx64ReleasePDB.zip">
    <img src=".github/assets/pdb2.png" alt="PDB" width="auto" height="auto">
  </a>
  <a href="https://nightly.link/mistakesmultiplied/neptune/workflows/msbuild/master/Amalgamx64ReleaseAVX2.zip">
    <img src=".github/assets/download_avx2.png" alt="Download AVX2" width="auto" height="auto">
  </a>
  <a href="https://nightly.link/mistakesmultiplied/neptune/workflows/msbuild/master/Amalgamx64ReleaseAVX2PDB.zip">
    <img src=".github/assets/pdb_avx2.png" alt="PDB AVX2" width="auto" height="auto">
  </a>
  <br>
  <a href="https://nightly.link/mistakesmultiplied/neptune/workflows/msbuild/master/Amalgamx64ReleaseFreetype.zip">
    <img src=".github/assets/freetype.png" alt="Download Freetype" width="auto" height="auto">
  </a>
  <a href="https://nightly.link/mistakesmultiplied/neptune/workflows/msbuild/master/Amalgamx64ReleaseFreetypePDB.zip">
    <img src=".github/assets/pdb2.png" alt="PDB Freetype" width="auto" height="auto">
  </a>
  <a href="https://nightly.link/mistakesmultiplied/neptune/workflows/msbuild/master/Amalgamx64ReleaseFreetypeAVX2.zip">
    <img src=".github/assets/freetype_avx2.png" alt="Download Freetype AVX2" width="auto" height="auto">
  </a>
  <a href="https://nightly.link/mistakesmultiplied/neptune/workflows/msbuild/master/Amalgamx64ReleaseFreetypeAVX2PDB.zip">
    <img src=".github/assets/pdb_avx2.png" alt="PDB Freetype AVX2" width="auto" height="auto">
  </a>
  <br>
  <a href="https://nightly.link/mistakesmultiplied/neptune/workflows/msbuild/master/Amalgamx64ReleaseTextmode.zip">
    <img src=".github/assets/textmode.png" alt="Download Textmode" width="auto" height="auto">
  </a>
  <a href="https://nightly.link/mistakesmultiplied/neptune/workflows/msbuild/master/Amalgamx64ReleaseTextmodePDB.zip">
    <img src=".github/assets/pdb2.png" alt="PDB Textmode" width="auto" height="auto">
  </a>
  <a href="https://nightly.link/mistakesmultiplied/neptune/workflows/msbuild/master/Amalgamx64ReleaseTextmodeAVX2.zip">
    <img src=".github/assets/textmode_avx2.png" alt="Download Textmode AVX2" width="auto" height="auto">
  </a>
  <a href="https://nightly.link/mistakesmultiplied/neptune/workflows/msbuild/master/Amalgamx64ReleaseTextmodeAVX2PDB.zip">
    <img src=".github/assets/pdb_avx2.png" alt="PDB Textmode AVX2" width="auto" height="auto">
  </a>
</p>

###### AVX2 may be faster than SSE2 though not all CPUs support it (Steam > Help > System Information > Processor Information > AVX2). Freetype uses freetype as the text rasterizer and includes some custom fonts, which results in better looking text but larger DLL sizes. Textmode disables amalgam's and some of the game's visuals. PDBs are for developer use.
NOTE: Textmode doesnt fully make the game textmode. You need to preload [TextModeTF2](https://github.com/TheGameEnhancer2004/TextmodeTF2) module to make the game textmode.
# Neptune

[![GitHub Repo stars](https://img.shields.io/github/stars/mistakesmultiplied/neptune)](/../../stargazers)
[![GitHub commit activity (branch)](https://img.shields.io/github/commit-activity/m/mistakesmultiplied/neptune)](/../../commits/)

[VAC bypass](https://github.com/danielkrupinski/VAC-Bypass-Loader) and [Xenos](https://github.com/DarthTon/Xenos/releases) recommended







# TF2 Chat Relay Discord Bot

A Discord bot that monitors Amalgam chat relay logs and posts TF2 chat messages to Discord in real-time.

## Features

🤖 **Real-time monitoring** of Amalgam chat logs  
💬 **Beautiful Discord embeds** with team colors and formatting  
🎨 **Team-specific styling** (RED/BLU colors, emojis)  
🔄 **Automatic file rotation** handling (daily log files)  
🚫 **Duplicate prevention** - no spam when multiple bots are running  
📊 **Status commands** to monitor bot health  
🛠️ **Error handling** with fallback messages  

## Preview

The bot will post messages like this in Discord:

```
🔴 PlayerName [RED]                           💬 TEAM
    gg guys, nice round
📡 valve_server_01 • 2024-01-15 14:30:25
```

## Setup Instructions

### 1. Create Discord Bot

1. Go to [Discord Developer Portal](https://discord.com/developers/applications)
2. Click "New Application" and give it a name (e.g., "TF2 Chat Relay")
3. Go to "Bot" section on the left
4. Click "Add Bot"
5. Copy the **Bot Token** (you'll need this later)
6. Enable these **Privileged Gateway Intents**:
   - ✅ Message Content Intent

### 2. Invite Bot to Server

1. In Developer Portal, go to "OAuth2" → "URL Generator"
2. Select scopes:
   - ✅ `bot`
3. Select bot permissions:
   - ✅ Send Messages
   - ✅ Use Slash Commands
   - ✅ Embed Links
   - ✅ Read Message History
4. Copy the generated URL and open it to invite the bot to your server

### 3. Get Channel ID

1. In Discord, enable Developer Mode:
   - User Settings → Advanced → Developer Mode ✅
2. Right-click the channel where you want chat messages
3. Click "Copy Channel ID"

### 4. Install Python Requirements

```bash
pip install -r requirements.txt
```

### 5. Configure Bot

**Option A: Using config.json (Recommended)**

Edit `config.json` and update these values:

```json
{
  "discord_token": "your_actual_bot_token_here",
  "channel_id": 987654321098765432
}
```

**Option B: Edit Python file directly**

Edit `discord_chat_relay.py` and update these lines:

```python
self.DISCORD_TOKEN = "YOUR_BOT_TOKEN_HERE"  # Replace with your bot token
self.CHANNEL_ID = 123456789012345678         # Replace with your channel ID
```

### 6. Run the Bot

**Windows (Easy):**
```bash
run_bot.bat
```

**Manual:**
```bash
python discord_chat_relay.py
```

## Bot Commands

### `!status`
Shows bot status, monitoring stats, and Amalgam directory status.

### `!logs [lines]`
Shows recent bot log entries (default: 10 lines, max: 50).

### `!reload` (Admin only)
Restarts the file monitoring task.

## Configuration

### Using config.json (Recommended)

The bot uses `config.json` for easy configuration. Here's the full structure:

```json
{
  "discord_token": "your_bot_token_here",
  "channel_id": 123456789012345678,
  "settings": {
    "check_interval_seconds": 2,        // How often to check for new messages
    "max_message_cache": 1000,          // Max messages to keep in memory
    "enable_debug_logging": false       // Enable detailed logging
  },
  "formatting": {
    "team_colors": {
      "RED": "0xff4444",               // Red team color (hex)
      "BLU": "0x4444ff",               // Blue team color
      "SPEC": "0x888888",              // Spectator color  
      "UNK": "0xcccccc"                // Unknown team color
    },
    "team_emojis": {
      "RED": "🔴",                     // Red team emoji
      "BLU": "🔵",                     // Blue team emoji
      "SPEC": "👁️",                    // Spectator emoji
      "UNK": "❓"                      // Unknown team emoji
    }
  }
}
```

### Legacy Configuration (Python file)

You can also modify these directly in the bot code:

```python
# In the ChatRelayBot.__init__() method
self.CHECK_INTERVAL = 2              # Check every 2 seconds
self.team_colors = {
    'RED': 0xff4444,                 # Red team color
    'BLU': 0x4444ff,                 # Blue team color
    'SPEC': 0x888888,                # Spectator color
    'UNK': 0xcccccc                  # Unknown team color
}
```

### Log Files Location

The bot monitors: `%APPDATA%\Amalgam\chat_relay_*.log`

## Troubleshooting

### Bot doesn't start
- ❌ **Token Error**: Make sure you set the correct bot token
- ❌ **Channel Error**: Verify the channel ID is correct
- ❌ **Permission Error**: Ensure bot has required permissions
- ❌ **Encoding Error**: Run `python fix_config.py` to fix config file issues

### No messages appearing
- 🔍 Use `!status` command to check if Amalgam directory is found
- 🔍 Make sure Chat Relay is enabled in Amalgam: `Misc → Automation → Chat Relay`
- 🔍 Check if TF2 is running and you're in a server with chat activity

### Bot shows "Missing Amalgam directory"
- 📁 The bot looks for: `C:\Users\YourName\AppData\Roaming\Amalgam\`
- 📁 Make sure Amalgam has created chat logs (join a server and see some chat)
- 📁 Verify the path exists and contains `chat_relay_*.log` files

### Multiple bots causing issues
- ✅ This is handled automatically! Only the "primary" bot logs messages
- ✅ The primary bot is determined by lowest entity index in-game
- ✅ No duplicates will appear even with 50+ bots in one game

### Config file encoding issues
- 🔧 **Run the fixer**: `python fix_config.py`
- 🔧 **Check manually**: Open config.json in a text editor and re-save as UTF-8
- 🔧 **Delete and recreate**: Delete config.json and run the bot to create a new one

## Example Log Format

The bot reads JSON logs that look like this:

```json
{"timestamp":"2024-01-15 14:30:25","server":"valve_server_01","player":"PlayerName","team":"RED","message":"gg everyone","teamchat":false}
{"timestamp":"2024-01-15 14:30:30","server":"valve_server_01","player":"TeamMate","team":"RED","message":"rush B","teamchat":true}
```

## Advanced Usage

### Running as Service (Windows)

You can run this bot as a Windows service using tools like:
- [NSSM (Non-Sucking Service Manager)](https://nssm.cc/)
- Task Scheduler

### Running 24/7 on VPS

For always-online monitoring:
1. Deploy to a VPS (DigitalOcean, AWS, etc.)
2. Use `screen` or `tmux` to keep it running
3. Set up automatic restart on reboot

### Multiple Servers

To monitor multiple TF2 servers:
1. Run multiple Amalgam instances on different machines
2. Each will create separate log files
3. One Discord bot can monitor all log files in the same directory

## File Structure

```
📁 TF2-Chat-Relay/
├── 📄 discord_chat_relay.py    # Main bot script
├── 📄 config.json              # Bot configuration (you create this)
├── 📄 fix_config.py            # Config troubleshooting tool
├── 📄 check_config.py          # Config validation script
├── 📄 requirements.txt         # Python dependencies  
├── 📄 run_bot.bat              # Easy Windows launcher
├── 📄 README.md               # This file
├── 📄 discord_bot.log         # Bot logs (auto-created)
└── 📁 %APPDATA%\Amalgam\      # Amalgam chat logs
    ├── 📄 chat_relay_20240115.log
    ├── 📄 chat_relay_20240116.log
    └── 📄 ...
```

## Support

If you encounter issues:

1. Check the `discord_bot.log` file for error messages
2. Use `!status` command to verify bot status
3. Ensure Amalgam is running and Chat Relay is enabled
4. Verify Discord bot permissions and channel access

## License

Free to use and modify.
