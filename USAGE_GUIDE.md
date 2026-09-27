# P4 Wall Keypad — Usage Guide

This guide is for **homeowners and installers** using a keypad that is already set up on your network.

---

## At a glance

| What | How |
|------|-----|
| **Control lights & covers** | Short press a button on the wall keypad |
| **Dim / color / position** | Hold the button ~½ second, then turn the knob |
| **Go back to all 10 buttons** | Press the **encoder knob** (center click) |
| **Settings & setup** | Web browser → `http://keypad.local` (or your keypad’s IP) |
| **Phone / PC control** | Same web address → **Control** tab |

---

## Opening the keypad in your browser

1. Make sure your phone or computer is on the **same home Wi‑Fi or LAN** as the keypad (Ethernet).
2. Open a browser and go to:
   - **`http://keypad.local`** — works on most phones and Macs, or  
   - **`http://192.168.x.x`** — use the IP your installer gave you if `.local` does not work.
3. Sign in with your **admin username and password** (not the factory default if the installer already changed it).
4. Use the top menu: **Config**, **Control**, **Network**, **HA Setup**.

**Sign out** when you are done on a shared computer (link in the top-right of each page).

---

## The wall keypad — screen layout

```
┌─────────────────────────────────────┐
│  P4 Wall Keypad    ETH  MQTT       │  ← status bar
├──────────────┬──────────────────────┤
│  Button 1    │  Button 6             │
│  [icon]      │  [icon]               │  ← left column = buttons 1–5
│  Name        │  Name                 │     right column = buttons 6–10
│  ...         │  ...                  │
│  Button 5    │  Button 10            │
└──────────────┴──────────────────────┘
         Rotary knob below the display
```

### Status bar (top)

| Indicator | Meaning |
|-----------|---------|
| **ETH UP** | Ethernet cable connected and working |
| **ETH DOWN** | No network — check cable and router |
| **MQTT UP** | Connected to your smart home broker |
| **MQTT DOWN** | Not talking to Home Assistant path — lights may not respond |

Both should be **UP** for normal use.

### Button tiles

Each tile shows:

- **Name** you chose in setup (e.g. “Kitchen”)
- **Icon** — bulb on/off for lights; blinds up/down for covers
- Empty or “(empty)” slots are not configured

**Physical layout matches the screen:** top-left is **Button 1**, bottom-left is **Button 5**, top-right is **Button 6**, bottom-right is **Button 10**.

---

## Controlling devices from the wall keypad

### Short press (tap)

| Button type | What happens |
|-------------|----------------|
| **On/Off** | Turns the light **on** or **off** |
| **Brightness** | Same — **toggle** on/off |
| **CCT** (warm/cool white) | Same — **toggle** on/off |
| **RGB** (color) | Same — **toggle** on/off |
| **Cover** (blinds/shades) | **Opens** if closed, **closes** if open |

Hold time for “long press” is about **half a second**.

### Long press (hold ~½ second)

Opens a **full-screen control** for that button only:

| Button type | Long press opens | What the knob does |
|-------------|------------------|---------------------|
| **Brightness** | Brightness screen | Turn = brighter/dimmer (0–100%) |
| **CCT** | Color + brightness screen | See [CCT mode](#cct-mode-warm--cool-white) below |
| **RGB** | Color + brightness screen | See [RGB mode](#rgb-mode-colored-lights) below |
| **Cover** | Position screen | Turn = move blinds 0–100% |
| **On/Off** | *(no extra screen)* | Use short press only |

### Exit any control screen

**Press the center of the rotary encoder** (push the knob). You return to the 10-button grid.

On-screen hint when in a menu: *“Rotate: adjust   Key: back”* (or Arabic equivalent if language is set to Arabic).

---

## CCT mode (warm / cool white)

For lights that support color temperature (Kelvin):

1. **Long press** the button you want to control.
2. You see brightness and color temperature controls.
3. **Short press physical Button 1** (top-left) → focus **brightness**.
4. **Short press physical Button 2** (second on the left) → focus **color temperature**.
5. **Turn the knob** to adjust the focused value.
6. **Push the knob** when finished.

> **Note:** Focus switching always uses **buttons 1 and 2** (left column, top two), even when you opened the menu from button 6–10 on the right.

Temperature range: roughly **2000 K** (warm) to **6500 K** (cool).

---

## RGB mode (colored lights)

For RGB lights:

1. **Long press** the button you want to control.
2. **Short press physical Button 1** → focus **brightness**.
3. **Short press physical Button 2** → focus **hue** (0–360°).
4. **Turn the knob** to adjust.
5. **Push the knob** to exit.

Same as CCT: only **buttons 1 and 2** change focus, regardless of which tile you long-pressed.

Saturation is fixed at full color in the current firmware (no separate saturation control on the keypad).

---

## Cover mode (blinds / shades)

1. **Short press** → open or close (whichever you are not doing now).
2. **Long press** → position screen; turn knob to set **0% (closed)** to **100% (open)**.
3. **Push the knob** to exit.

The icon shows **open** when position is above about 10%, **closed** at 10% or below.

---

## Using the web interface

### Control (daily use)

Best for phone or tablet on the couch:

- **Room Presence** card at the bottom (mmWave radar) — occupied / empty.
- Shows the same **10 buttons** as the wall unit.
- Tap a card to **toggle** a light or open/close a cover.
- **Sliders** appear for brightness, color temp, hue, or cover position — drag to adjust.
- Page refreshes automatically; states should match Home Assistant within a few seconds.

If you set **Arabic** in Config, the Control page uses **right-to-left** layout and Arabic labels. Config itself stays in English.

### Config (setup and changes)

Use when you need to change what a button does:

| Section | What you change |
|---------|-----------------|
| **MQTT** | Broker address (usually only once) |
| **Web Login** | Admin password — use a strong password; leave password fields **blank** to keep the current password |
| **Language** | English or Arabic (LCD + Control page) |
| **Button assignments** | Name, Home Assistant entity ID, type per button |

After **Save Configuration**, the keypad **reboots** (~30 seconds). Wait before pressing buttons.

**Warning on screen:** If you still see “factory default password”, change the admin password before leaving the install.

### Network

- Switch between **DHCP** (automatic IP) and **static IP** if your installer reserved an address.
- Change **hostname** (default `keypad`) — affects `http://keypad.local`.
- Save → reboot.

### HA Setup

For **installers** linking Home Assistant:

1. Configure all buttons on **Config** and save.
2. Open **HA Setup** → **Regenerate from saved config**.
3. Copy the three YAML blocks into Home Assistant (script + two automations).
4. Enable them in HA.

Homeowners usually do not need this page after initial install unless buttons are reassigned.

---

## Common tasks

### Change what a button controls

1. Browser → **Config**.
2. Edit that button’s **name**, **entity ID**, and **type**.
3. **Save Configuration** (device reboots).
4. Installer: **HA Setup** → Regenerate → update the “Keypad Sync” automation in HA if entities changed.

### Change admin password

1. **Config** → **Web Login**.
2. Enter username (or leave as is), **new password**, **confirm password** (min 8 characters).
3. Save → reboot → log in with the new password.

### Switch display language to Arabic

1. **Config** → **Language** → **العربية**.
2. Save → reboot.
3. LCD and **Control** web page use Arabic; setup pages stay English.

### Control from phone while away from the wall

Same network only: open **Control** in the browser. The keypad web UI does not work over the internet unless your network is set up for that (not recommended by default).

### One tile does not match the real light

- Wait a few seconds (state comes from Home Assistant via MQTT).
- Check **MQTT** is **UP** on the keypad.
- Ask your installer to verify the **entity ID** on Config matches HA exactly (e.g. `light.kitchen`, not `light.kitchen_2`).
- Toggle the light from the HA app once — sync automation should update the tile.

### All buttons dead but screen is on

- Check **ETH** and **MQTT** on the status bar.
- Power-cycle the keypad (unplug/replug or switch off the supply).
- If MQTT stays DOWN, broker address or password may be wrong — fix on **Config** (installer).

---

## What the keypad does *not* do

- **No touch screen** — use physical buttons and the knob only on the wall.
- **No Wi‑Fi setup on the keypad itself** — it uses **Ethernet** to your router.
- **No direct Home Assistant app** — it speaks MQTT; HA automations do the real work.
- **No internet remote access** out of the box — LAN only.

---

## Quick reference — gestures

| Gesture | Result |
|---------|--------|
| Short press button | Toggle light, or open/close cover |
| Hold button ~½ s | Open brightness / CCT / RGB / cover menu |
| Turn encoder | Adjust value in menu |
| Push encoder | Back to 10-button grid |
| Short press **Button 1** (in CCT/RGB menu) | Select brightness row |
| Short press **Button 2** (in CCT/RGB menu) | Select color temp or hue row |

---

*P4 Wall Keypad — Usage Guide*
