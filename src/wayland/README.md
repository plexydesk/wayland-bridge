# Wayland Bridge for PlexyShell

This directory contains the Wayland protocol bridge implementation that allows standard Wayland applications to run on PlexyShell without any modifications to the compositor.

## What is it?

The Wayland Bridge acts as a protocol translator:
- **Server side**: Implements a standard Wayland compositor (using libwayland-server)
- **Client side**: Connects to PlexyShell as a normal Plexy client (using libplexy)

This allows any Wayland application (Firefox, foot terminal, GTK/Qt apps) to run on PlexyShell.

## Architecture

```
Wayland Apps → Wayland Bridge → PlexyShell
(wl_surface)   (translator)     (native protocol)
```

See [../docs/WAYLAND_BRIDGE.md](../docs/WAYLAND_BRIDGE.md) for detailed architecture documentation.

## Building

The bridge is built automatically with:
```bash
make wayland_bridge
```

Requirements:
- `libwayland-server` - Wayland compositor library
- `wayland-protocols` - Standard protocol definitions
- `libplexy.so` - Plexy client library (built automatically)

## Running

1. Start PlexyShell:
```bash
./plexyshell
```

2. In another terminal, start the Wayland bridge:
```bash
./run-wayland-bridge.sh
# Or directly:
./wayland_bridge
```

3. Launch Wayland applications:
```bash
# The bridge will set WAYLAND_DISPLAY automatically
# Simple test apps:
weston-simple-shm
weston-terminal

# Real applications:
foot
firefox
mpv --gpu-context=wayland
```

## Implementation Status

For protocol-level status, use [`docs/wayland_protocol_matrix.md`](../docs/wayland_protocol_matrix.md) as the source of truth.

### ✅ Implemented (high level)
- Core Wayland globals and lifecycle (`wl_compositor`, `wl_shm`, `wl_output`, `xdg_wm_base`)
- Input forwarding for pointer/keyboard (`wl_seat`) and relative/constraint helpers
- Data paths for clipboard/selection (`wl_data_device_manager`, primary-selection, ext-data-control)
- DMA-BUF + SHM buffer paths
- Output/HiDPI metadata (`wl_output`, `zxdg_output_manager_v1`, fractional scale)
- Activation/presentation/viewporter support
- Xwayland integration path (`xwayland_shell_v1` via XWM)

### 🚧 Partial / in progress
- `wl_subcompositor` stacking ops (`place_above` / `place_below`)
- Touch: single-touch emulation only (`down`/`up`/`cancel`), no touch `motion`/multitouch
- Text input: basic `zwp_text_input_manager_v3`, no input-method-v2
- Decorations and cursor-shape behavior are functional but limited
- Pointer gestures use bridge-side heuristics (not true multitouch backend)

## Files

- `wayland_bridge.c` - Main bridge program and event loop
- `wayland_bridge.h` - Bridge data structures
- `weston_compositor.c` - Weston libweston integration wrapper
- `weston_compositor.h` - Weston integration API
- `wl_compositor.c` - wl_compositor and wl_surface implementation
- `wl_shm.c` - Shared memory buffer handling
- `xdg_shell.c` - XDG shell protocol (window management)
- `wl_seat.c` - Input device handling
- `wl_output.c` - Display output information
- `wl_data_device.c` - Data device manager (clipboard/DnD paths)
- `wl_text_input.c` - Text input protocol (basic v3 behavior)
- `wl_pointer_gestures.c` - Pointer gestures protocol objects + event emission
- `xwm.c` - XWM bridge for Xwayland integration
- `event_forward.c` - Event forwarding from Plexy to Wayland clients
- `xdg-shell-protocol.[ch]` - Generated XDG shell protocol code
- `text-input-v3-protocol.[ch]` - Generated text input protocol code

## Testing

Simple tests:
```bash
# Animated bouncing ball (tests rendering)
weston-simple-shm

# Terminal (tests text rendering and input)
weston-terminal
foot

# Check for memory leaks
valgrind --leak-check=full ./wayland_bridge
```

## Limitations

- **Xwayland startup contract is basic**: currently uses display lock probing plus startup delay; no `-displayfd` handshake yet
- **Client-side decorations**: Wayland apps usually draw their own title bars; Plexy's decorations should be disabled for these windows
- **Single output**: Only reports one output to clients currently
- **Touch input**: single-touch emulation only, currently no touch `motion` forwarding
- **Gesture backend**: pointer gestures are heuristic and not hardware multitouch

## Performance

- **Zero-copy buffers**: SHM file descriptors are shared directly between client→bridge→PlexyShell
- **Event latency**: ~2 IPC hops (comparable to other Wayland compositors)
- **Memory overhead**: ~600 bytes per window, negligible

## Troubleshooting

**"Failed to connect to PlexyShell"**
- Make sure `plexyshell` is running first
- Check that `/tmp/plexy.sock` exists

**"No protocol is available"**
- Update wayland-protocols package
- Regenerate protocol files: `make clean && make wayland_bridge`

**Black windows or no rendering**
- Check if app supports wl_shm (most do)
- Try with `weston-simple-shm` first to verify bridge works

**Input not working**
- Keyboard: Check if keymap is being sent properly
- Pointer: Verify focus is being set on window enter

## Weston Integration

The bridge now includes optional Weston compositor integration for advanced protocol handling:

### Files
- `weston_compositor.c/h` - Weston libweston integration wrapper

### Compatibility
- **libweston version**: Arch currently ships Weston 14.0.2 / `libweston-14`; the build also detects newer libweston APIs when present
- **Backend**: Headless backend with Pixman renderer (software rendering)
- **Purpose**: Provides robust implementations of complex Wayland protocols

### Configuration
The Weston integration uses:
- `WESTON_RENDERER_PIXMAN` - Software renderer (no GPU required)
- Headless backend - No real display output
- Refresh rate: 60 Hz

### API Compatibility Notes
All APIs are compatible with the Arch-targeted `libweston-14` baseline:
- ✅ `weston_compositor_create()` - Uses correct signature with user_data
- ✅ `weston_compositor_load_backend()` - Properly configured headless backend
- ✅ `weston_log_set_handler()` - Standard logging interface
- ✅ Signal usage - `output_created_signal` and `create_surface_signal` are correct
- ✅ Backend config - Uses `WESTON_HEADLESS_BACKEND_CONFIG_VERSION 3`

To compare vendored source snapshots against the current Arch package set, run `make arch-sync-status`.

## Future Work

1. **Xwayland hardening** - add `-displayfd`/readiness handshake and lifecycle robustness
2. **Layer shell** - map `zwlr_layer_shell_v1` to Plexy layer surfaces
3. **Touch/tablet/input-method** - fill missing protocol families
4. **Multi-output** - expose multiple monitors if Plexy supports them
5. **Enable Weston mode** - switch from manual protocol handling to Weston-managed protocols where useful
