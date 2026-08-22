# 27_V4l2Ctl

Opt-in Linux V4L2 on top of the usual `cam.begin()` / `capture()` path.

```cpp
cam.begin(board);
v4l.begin(&cam);            // /dev/video0 on CSI
jpeg.begin(cam.width(), cam.height());
h264.begin(cam.width(), cam.height());
v4l2m.begin({&cam, &jpeg, &h264});  // /dev/video10–12, /dev/video20
v4l.ctl("--list-ctrls");
ioctl(v4l.fd(), VIDIOC_G_CTRL, &c);
camera_fb_t *fb = cam.capture();
```

M2M nodes: `/dev/video10` JPEG enc, `/dev/video11` H.264 enc, `/dev/video12` JPEG dec, `/dev/video20` ISP stats + blob CIDs.

Zero-copy V4L2 MMAP: `v4l.mmap(length, offset)` after `VIDIOC_QUERYBUF`. POSIX `mmap()` is ENOSYS on ESP-IDF VFS. SPI1 cameras use `/dev/video4`.

Serial @ 115200 accepts v4l2-ctl flags: `--list-ctrls`, `--list-formats`, `--set-ctrl gain=16`, `--get-fmt-video`, `--stream-count=1`.

Do not start Arduino `ESP_Video` on the same CSI host. Do not mix `cam.capture()` and `VIDIOC_DQBUF` / `--stream-count` in the same loop (same FB pool).
