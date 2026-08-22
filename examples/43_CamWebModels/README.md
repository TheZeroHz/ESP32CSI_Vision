# 43_CamWebModels

The sketch **owns** `camera_fb_t`. The library does not capture in a hidden worker.

```text
cam.capture() → det.infer(fb) → det.draw(fb) → preview.present(fb) → cam.release(fb)
```

Browser: `http://<ip>/` live MJPEG, `/dets` JSON.

## Board

Not Guition-only. Set camera + Wi-Fi in **`board_config.h`** in this folder.  
Guide: [docs/Custom-Boards.md](../../docs/Custom-Boards.md). Flash `00_BoardConfig` first.

## Model

Copy `models/espdl/p4/*.espdl` to `/models/p4/` on SD or flash. Change `APP_MODEL` in the sketch.
