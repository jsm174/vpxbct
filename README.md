# VPX Batocera Capture Tool

A utility for capturing backglass and playfield images and videos from your Visual Pinball X (VPX) Standalone cabinet running Batocera 42.

## Features

- Preview and capture the current playfield and backglass in real time
- Edit game metadata (name, description, etc.)
- Update JSON metadata for each table
- Generate descriptions using OpenAI (optional)

## Screenshots

![Main Controls](./img/main.jpg)  
*Reload games, capture media, or launch tables directly*

![Live Preview](./img/live-preview.jpg)  
*See what's on your cabinet if you are away from it*

![Editor View](./img/editor.jpg)  
*Edit table names, descriptions, and metadata inline with JSON view*



## Settings

```ini
[Settings]
ESURL=http://your-es-ip:port
OpenAIKey=sk-XXXXXXXXXXXXXXXXXXXXXX
```

- `ESURL`: URL to your EmulationStation frontend *(leave blank)*
- `OpenAIKey`: Optional. Enables AI-powered table description generation *(experimental)*

## Installation

1. Download the tool into this folder on your Batocera box:  
   `/userdata/system/configs/vpinball`
2. On your cabinet, go to EmulationStation > Developer > Frontend Developer Options
3. Make sure **Enable Public Web API Access** is turned on
4. From another device on the same network, open a browser to:  
   `http://<your-batocera-ip>:8111`
5. Start capturing and editing!

## Shoutouts

Thanks to the entire Batocera team, especially @susan34, @dmanlfc, and @maximumentropy!