# Benchmark host

The benchmark host is also a gaming PC (CachyOS, KDE Plasma, Steam), so benchmarks run from a boot
entry of their own instead of retuning the system each time. The normal boot is untouched: the
bench entry is only used when picked explicitly, and only for that one boot.

Written for an AMD Ryzen 7 1700 (Zen 1, 8 cores / 16 threads, SMT siblings are cpu N and N+8)
booted by systemd-boot, with Cool'n'Quiet on in the BIOS and MSI Game Boost off.

## What the bench boot does

- Boots headless into `bench.target`: `multi-user.target` without the display manager (so no
  desktop session and no Steam), `ananicy-cpp`, `power-profiles-daemon` or `scx_loader`. SSH and
  the network come up as usual.
- Gives CCX1 (cpus `4-7,12-15`, with its own L3) to the benchmarks: `isolcpus`, `nohz_full` and
  `rcu_nocbs` keep tasks, timer ticks and RCU callbacks off it, and `irqaffinity` sends
  interrupts to CCX0 (`0-3,8-11`).
- Holds the clock at a fixed 3.0 GHz, the base clock: the `performance` governor with boost off,
  so results don't depend on temperature or on how many cores are busy.
- Queues the normal entry for the next boot: `loader.conf` has `default @saved`, which would
  otherwise keep coming back to the bench entry.

## Install

It adds three files and edits nothing:

- `/etc/systemd/system/bench.target`
- `/etc/systemd/system/bench-tune.service`, started only by `bench.target`
- `/boot/loader/entries/bench-linux-cachyos.conf`, a copy of `linux-cachyos.conf` plus the
  options above

```bash
sudo tools/bench-host/bench-host.sh install
```

`sdboot-manage` only rewrites entries named after a kernel and only removes entries whose kernel
was uninstalled, so the bench entry survives kernel updates. Run `install` again if the normal
entry's options change.

## Use

```bash
sudo systemctl reboot --boot-loader-entry=bench-linux-cachyos.conf
```

Then, over SSH and inside `tmux` so a dropped connection doesn't end the run, pin each benchmark to
one core of CCX1 and leave its SMT sibling idle:

```bash
taskset -c 4 setarch -R build/tests/benchmark/quantcup_score_node-indexedbook
```

`setarch -R` turns off address space randomisation for that process only.

Leave with `sudo reboot`, or a power cycle: both land in the normal boot.

## Check

After the first bench boot:

```bash
sudo tools/bench-host/bench-host.sh status
```

It shows what's installed and, in the bench boot, the governor, boost, isolated cpus and the
kernel workqueue cpumask. If the workqueue mask isn't `0f0f`, unbound kernel work still runs on
CCX1; add a line writing `0f0f` to `/sys/devices/virtual/workqueue/cpumask` in
`bench-tune.service`.

A busy loop on cpu 4 should hold about 3000 MHz:

```bash
taskset -c 4 sh -c 'while :; do :; done' & sleep 3; for i in 1 2 3 4 5; do awk '/^processor/{p=$3} /MHz/ && p==4' /proc/cpuinfo; sleep 1; done; kill %1
```

## Remove

```bash
sudo tools/bench-host/bench-host.sh uninstall
```

It removes the three files (only if they were written by `install`) and reloads systemd. Nothing
else needs undoing. In the bench boot, `sudo systemctl isolate graphical.target` brings the desktop
back without rebooting, but the cpu isolation stays until the next boot.
