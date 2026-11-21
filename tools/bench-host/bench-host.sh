#!/usr/bin/env bash
# Installs, removes or checks the headless benchmark boot mode. See README.md.
#
# BENCH_ESP, BENCH_UNIT_DIR, BENCH_SYSTEMCTL and BENCH_BOOTCTL point the script at another tree,
# for testing it without root.
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
esp=${BENCH_ESP:-/boot}
unit_dir=${BENCH_UNIT_DIR:-/etc/systemd/system}
systemctl=${BENCH_SYSTEMCTL:-systemctl}
bootctl=${BENCH_BOOTCTL:-bootctl}

marker='# Managed by trading-systems tools/bench-host'
base_entry=$esp/loader/entries/linux-cachyos.conf
entry=$esp/loader/entries/bench-linux-cachyos.conf

# CCX1 runs the benchmarks with its own L3; CCX0 takes everything else.
bench_cpus=4-7,12-15
housekeeping_cpus=0-3,8-11
bench_options="systemd.unit=bench.target"
bench_options+=" isolcpus=managed_irq,domain,$bench_cpus nohz_full=$bench_cpus rcu_nocbs=$bench_cpus"
bench_options+=" irqaffinity=$housekeeping_cpus skew_tick=1 nmi_watchdog=0"
bench_options+=" cpufreq.default_governor=performance"

files=("$unit_dir/bench.target" "$unit_dir/bench-tune.service" "$entry")

die() {
  echo "bench-host: $*" >&2
  exit 1
}

# Refuses to overwrite or remove a file this script didn't write.
check_ours() {
  [[ ! -e $1 ]] || grep -qxF "$marker" "$1" || die "$1 exists and wasn't written by bench-host"
}

cmd_install() {
  [[ -f $base_entry ]] || die "$base_entry not found"
  local f
  for f in "${files[@]}"; do check_ours "$f"; done

  # Same kernel, initrd and root as the normal entry, minus the boot splash.
  local words=() options=() w
  read -r -a words <<<"$(awk '$1 == "options" { $1 = ""; print }' "$base_entry")"
  for w in "${words[@]}"; do
    case $w in quiet | splash) ;; *) options+=("$w") ;; esac
  done
  [[ ${#options[@]} -gt 0 ]] || die "no options line in $base_entry"

  cp "$here/bench.target" "$here/bench-tune.service" "$unit_dir/"
  {
    echo "$marker"
    echo "title Linux CachyOS (bench)"
    grep -E '^(linux|initrd)[[:space:]]' "$base_entry"
    echo "options ${options[*]} $bench_options"
  } >"$entry"
  "$systemctl" daemon-reload
  # loader.conf has `default @saved`, which makes a bench entry picked in the boot menu the
  # default for every boot after it. A default set here overrides loader.conf and is never saved
  # over, so every boot that isn't picked explicitly is the normal one.
  "$bootctl" set-default "${base_entry##*/}"

  echo "Installed. Boot into it once with:"
  echo "  sudo systemctl reboot --boot-loader-entry=${entry##*/}"
}

cmd_uninstall() {
  local f
  for f in "${files[@]}"; do check_ours "$f"; done
  rm -f "${files[@]}"
  "$systemctl" daemon-reload
  "$bootctl" set-default ''
  echo "Removed."
}

show() {
  printf '  %-10s %-16s (want %s)\n' "$1" "$(cat "$2" 2>/dev/null || echo '?')" "$3"
}

cmd_status() {
  local f
  for f in "${files[@]}"; do
    printf '%-9s %s\n' "$([[ -e $f ]] && echo installed || echo missing)" "$f"
  done
  if grep -qw 'systemd.unit=bench.target' /proc/cmdline 2>/dev/null; then
    echo "Booted in bench mode:"
    show governor /sys/devices/system/cpu/cpu4/cpufreq/scaling_governor performance
    show boost /sys/devices/system/cpu/cpufreq/boost 0
    show isolated /sys/devices/system/cpu/isolated "$bench_cpus"
    show workqueue /sys/devices/virtual/workqueue/cpumask 0f0f
    printf '  %-10s %s\n' bench-tune "$("$systemctl" is-active bench-tune.service || true)"
  else
    echo "Not booted in bench mode."
  fi
}

[[ $EUID -eq 0 || -n ${BENCH_ESP:-} ]] || die "run with sudo"
case ${1:-} in
install) cmd_install ;;
uninstall) cmd_uninstall ;;
status) cmd_status ;;
*) die "usage: $0 install|uninstall|status" ;;
esac
